#!/usr/bin/env python3
"""
benchmark.py - Script de automação experimental e coleta de métricas para BCC203.

Este script executa os benchmarks conforme a metodologia científica estabelecida
no relatório prático (relatorio.tex):
1. Avalia 4 métodos de pesquisa externa (ISA, Árvore Binária, Árvore B, Árvore B*).
2. Avalia 3 estados de arquivo (Ascendente, Descendente, Desordenado).
3. Avalia 5 ordens volumétricas N in {20, 2000, 20000, 200000, 2000000}.
4. Avalia sob regimes Cold Start e Warm Start com controle rigoroso de cache.
5. Executa 100 repetições válidas sob idênticas condições.
6. Garante a amostragem de chaves estritamente contidas no arquivo de entrada.
7. Registra métricas de tempo (p50, p75, p99), leituras de disco e comparações (p50).
8. Salva os dados em formato CSV bruto (todas as execuções) e consolidado (sumário).
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import random
import re
import statistics
import struct
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# Constantes estruturais dos registros do BCC203
ITEM_SIZE_BYTES = 5016  # sizeof(Item): int32 key (4) + pad (4) + int64 value (8) + 5000 chars

# Nomes e mapeamentos descritivos
METHOD_NAMES: Dict[int, str] = {
    1: "ISA",
    2: "Arvore_Binaria",
    3: "Arvore_B",
    4: "Arvore_B_Estrela",
}

METHOD_LATEX_NAMES: Dict[int, str] = {
    1: "Acesso Sequencial Indexado (ISA)",
    2: "Árvore Binária de Pesquisa Externa",
    3: "Árvore B",
    4: "Árvore B*",
}

SITUATION_NAMES: Dict[int, str] = {
    1: "Ascendente",
    2: "Descendente",
    3: "Desordenado",
}

SITUATION_FILES: Dict[int, str] = {
    1: "items_ascending.bin",
    2: "items_descending.bin",
    3: "items_shuffled.bin",
}

METHOD_CACHE_EXTENSIONS: Dict[int, List[str]] = {
    1: [".cache_01"],
    2: [".binarytree"],
    3: [".btree"],
    4: [".bstartree"],
}

DEFAULT_QUANTITIES = [20, 2000, 20000, 200000, 2000000]

# Expressões regulares para parsing da saída de pesquisa
RE_FOUND = re.compile(r"Found key:\s*\{")
RE_NOT_FOUND = re.compile(r"Key not found")
RE_PREP_TIME = re.compile(r"Preprocessing time:\s*([0-9.]+)\s*ms")
RE_PREP_READS = re.compile(r"Preprocessing reads from disk to RAM:\s*([0-9]+)")
RE_PREP_COMPS = re.compile(r"Preprocessing key comparisons:\s*([0-9]+)")
RE_SEARCH_TIME = re.compile(r"Search time:\s*([0-9.]+)\s*ms")
RE_SEARCH_READS = re.compile(r"Search reads from disk to RAM:\s*([0-9]+)")
RE_SEARCH_COMPS = re.compile(r"Search key comparisons:\s*([0-9]+)")
RE_UNSUPPORTED = re.compile(r"unsuported", re.IGNORECASE)
RE_METHOD_NOT_IMPL = re.compile(r"must be one of these", re.IGNORECASE)


@dataclass
class SingleRunResult:
    method_id: int
    method_name: str
    situation_id: int
    situation_name: str
    quantity: int
    regime: str
    run_index: int
    key: int
    prep_time_ms: float
    prep_reads: int
    prep_comps: int
    search_time_ms: float
    search_reads: int
    search_comps: int
    status: str  # OK, TIMEOUT, NOT_FOUND, ERROR, UNSUPPORTED


@dataclass
class SummaryRow:
    method_id: int
    method_name: str
    situation_id: int
    situation_name: str
    regime: str
    quantity: int
    prep_p50_ms: Optional[float]
    prep_p75_ms: Optional[float]
    prep_p99_ms: Optional[float]
    prep_reads_p50: Optional[int]
    prep_comps_p50: Optional[int]
    search_p50_ms: Optional[float]
    search_p75_ms: Optional[float]
    search_p99_ms: Optional[float]
    search_reads_p50: Optional[int]
    search_comps_p50: Optional[int]
    valid_runs: int
    status: str


def compute_percentiles(values: List[float]) -> Tuple[float, float, float]:
    """Calcula os percentis p50 (mediana), p75 e p99 usando interpolação linear padrão."""
    if not values:
        return 0.0, 0.0, 0.0
    sorted_v = sorted(values)
    n = len(sorted_v)

    def get_p(p: float) -> float:
        if n == 1:
            return sorted_v[0]
        idx = (p / 100.0) * (n - 1)
        low = int(math.floor(idx))
        high = int(math.ceil(idx))
        weight = idx - low
        return sorted_v[low] * (1.0 - weight) + sorted_v[high] * weight

    return get_p(50.0), get_p(75.0), get_p(99.0)


def compute_median_int(values: List[int]) -> int:
    """Calcula a mediana inteira de grandezas discretas (leituras e comparações)."""
    if not values:
        return 0
    return int(round(statistics.median(values)))


class BenchmarkSuite:
    def __init__(
        self,
        executable_path: Path,
        tmp_dir: Path,
        output_dir: Path,
        methods: List[int],
        situations: List[int],
        quantities: List[int],
        regimes: List[str],
        runs_per_config: int = 100,
        timeout_seconds: float = 60.0,
        skip_on_timeout: bool = True,
        seed: Optional[int] = None,
        verbose: bool = False,
    ):
        self.executable_path = executable_path.resolve()
        self.tmp_dir = tmp_dir.resolve()
        self.output_dir = output_dir.resolve()
        self.methods = methods
        self.situations = situations
        self.quantities = quantities
        self.regimes = regimes
        self.runs_per_config = runs_per_config
        self.timeout_seconds = timeout_seconds
        self.skip_on_timeout = skip_on_timeout
        self.verbose = verbose

        if seed is not None:
            random.seed(seed)

        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.raw_csv_path = self.output_dir / "benchmark_raw.csv"
        self.summary_csv_path = self.output_dir / "benchmark_summary.csv"

        self._init_csv_files()

    def _init_csv_files(self) -> None:
        """Inicializa os cabeçalhos dos arquivos CSV se não existirem."""
        if not self.raw_csv_path.exists():
            with open(self.raw_csv_path, "w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow([
                    "method_id",
                    "method_name",
                    "situation_id",
                    "situation_name",
                    "quantity",
                    "regime",
                    "run_index",
                    "key",
                    "prep_time_ms",
                    "prep_reads",
                    "prep_comps",
                    "search_time_ms",
                    "search_reads",
                    "search_comps",
                    "status",
                ])

        if not self.summary_csv_path.exists():
            with open(self.summary_csv_path, "w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow([
                    "method_id",
                    "method_name",
                    "situation_id",
                    "situation_name",
                    "regime",
                    "quantity",
                    "prep_p50_ms",
                    "prep_p75_ms",
                    "prep_p99_ms",
                    "prep_reads_p50",
                    "prep_comps_p50",
                    "search_p50_ms",
                    "search_p75_ms",
                    "search_p99_ms",
                    "search_reads_p50",
                    "search_comps_p50",
                    "valid_runs",
                    "status",
                ])

    def sample_key(self, situation_id: int, quantity: int) -> int:
        """
        Garante a extração de uma chave estritamente presente no arquivo de entrada
        dentro da janela dos primeiros `quantity` registros.
        """
        filename = SITUATION_FILES[situation_id]
        filepath = self.tmp_dir / filename

        if not filepath.exists():
            # Fallback seguro para chaves determinísticas se o arquivo binário ainda não existir
            if situation_id == 1:
                return random.randint(0, quantity - 1)
            elif situation_id == 2:
                return 2000000 - 1 - random.randint(0, quantity - 1)
            else:
                return random.randint(0, quantity - 1)

        rand_idx = random.randint(0, quantity - 1)
        byte_offset = rand_idx * ITEM_SIZE_BYTES

        try:
            with open(filepath, "rb") as f:
                f.seek(byte_offset)
                raw_bytes = f.read(4)
                if len(raw_bytes) == 4:
                    return struct.unpack("<i", raw_bytes)[0]
        except OSError as e:
            if self.verbose:
                print(f"[Aviso] Falha ao ler chave no offset {byte_offset}: {e}", file=sys.stderr)

        return rand_idx

    def get_cache_paths(self, method_id: int, situation_id: int) -> List[Path]:
        """Retorna os caminhos dos arquivos de cache/índice associados à configuração."""
        filename = SITUATION_FILES[situation_id]
        base_path = self.tmp_dir / filename
        extensions = METHOD_CACHE_EXTENSIONS.get(method_id, [])
        return [Path(str(base_path) + ext) for ext in extensions]

    def remove_caches(self, method_id: int, situation_id: int) -> None:
        """Remove expressamente os arquivos de cache/índice para garantir Cold Start limpo."""
        for p in self.get_cache_paths(method_id, situation_id):
            if p.exists():
                try:
                    p.unlink()
                except OSError as e:
                    if self.verbose:
                        print(f"[Aviso] Falha ao deletar arquivo de cache {p}: {e}", file=sys.stderr)

    def cache_exists(self, method_id: int, situation_id: int) -> bool:
        """Verifica se ao menos um arquivo de cache correspondente existe."""
        for p in self.get_cache_paths(method_id, situation_id):
            if p.exists() and p.stat().st_size > 0:
                return True
        return False

    def run_executable(
        self, method_id: int, quantity: int, situation_id: int, key: int
    ) -> Tuple[int, str, str, float]:
        """Invoca o binário 'pesquisa' com os argumentos e mede o tempo de parede."""
        cmd = [
            str(self.executable_path),
            str(method_id),
            str(quantity),
            str(situation_id),
            str(key),
        ]
        t0 = time.perf_counter()
        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=self.timeout_seconds,
            cwd=str(self.executable_path.parent.parent),  # Raiz do projeto
        )
        dt = time.perf_counter() - t0
        return proc.returncode, proc.stdout, proc.stderr, dt

    def parse_output(
        self, returncode: int, stdout: str, stderr: str
    ) -> Tuple[str, Dict[str, Any]]:
        """Analisa a saída padrão e erros do executável 'pesquisa'."""
        combined = stdout + "\n" + stderr

        if RE_UNSUPPORTED.search(combined):
            return "UNSUPPORTED", {}

        if RE_METHOD_NOT_IMPL.search(combined):
            return "NOT_IMPLEMENTED", {}

        if returncode != 0:
            return "ERROR", {"error": combined.strip()}

        if RE_NOT_FOUND.search(combined) and not RE_FOUND.search(combined):
            return "NOT_FOUND", {}

        if not RE_FOUND.search(combined):
            return "ERROR", {"error": "Chave não confirmada na saída"}

        m_prep_t = RE_PREP_TIME.search(combined)
        m_prep_r = RE_PREP_READS.search(combined)
        m_prep_c = RE_PREP_COMPS.search(combined)
        m_search_t = RE_SEARCH_TIME.search(combined)
        m_search_r = RE_SEARCH_READS.search(combined)
        m_search_c = RE_SEARCH_COMPS.search(combined)

        if not all([m_prep_t, m_prep_r, m_prep_c, m_search_t, m_search_r, m_search_c]):
            return "ERROR", {"error": "Formato de métricas incompleto na saída padrão"}

        data = {
            "prep_time_ms": float(m_prep_t.group(1)),
            "prep_reads": int(m_prep_r.group(1)),
            "prep_comps": int(m_prep_c.group(1)),
            "search_time_ms": float(m_search_t.group(1)),
            "search_reads": int(m_search_r.group(1)),
            "search_comps": int(m_search_c.group(1)),
        }
        return "OK", data

    def record_raw_result(self, res: SingleRunResult) -> None:
        """Salva imediatamente um resultado de execução no arquivo CSV bruto."""
        with open(self.raw_csv_path, "a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow([
                res.method_id,
                res.method_name,
                res.situation_id,
                res.situation_name,
                res.quantity,
                res.regime,
                res.run_index,
                res.key,
                f"{res.prep_time_ms:.6f}",
                res.prep_reads,
                res.prep_comps,
                f"{res.search_time_ms:.6f}",
                res.search_reads,
                res.search_comps,
                res.status,
            ])

    def record_summary_row(self, row: SummaryRow) -> None:
        """Salva a consolidação estatística da configuração no CSV sumário."""
        with open(self.summary_csv_path, "a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow([
                row.method_id,
                row.method_name,
                row.situation_id,
                row.situation_name,
                row.regime,
                row.quantity,
                f"{row.prep_p50_ms:.4f}" if row.prep_p50_ms is not None else "",
                f"{row.prep_p75_ms:.4f}" if row.prep_p75_ms is not None else "",
                f"{row.prep_p99_ms:.4f}" if row.prep_p99_ms is not None else "",
                row.prep_reads_p50 if row.prep_reads_p50 is not None else "",
                row.prep_comps_p50 if row.prep_comps_p50 is not None else "",
                f"{row.search_p50_ms:.4f}" if row.search_p50_ms is not None else "",
                f"{row.search_p75_ms:.4f}" if row.search_p75_ms is not None else "",
                f"{row.search_p99_ms:.4f}" if row.search_p99_ms is not None else "",
                row.search_reads_p50 if row.search_reads_p50 is not None else "",
                row.search_comps_p50 if row.search_comps_p50 is not None else "",
                row.valid_runs,
                row.status,
            ])

    def run_benchmark_configuration(
        self, method_id: int, situation_id: int, quantity: int, regime: str
    ) -> SummaryRow:
        """
        Executa as repetições para uma dada configuração (Algoritmo, Situação, Qtd, Regime).
        Garante as 100 execuções válidas com a chave estritamente presente.
        """
        method_name = METHOD_NAMES[method_id]
        situation_name = SITUATION_NAMES[situation_id]
        print(f"\n>> [{method_name}] Situação: {situation_name} | N={quantity:,} | Regime: {regime.upper()}")

        # Regra estrutural do ISA: suporta exclusivamente situação 1 (Ascendente)
        if method_id == 1 and situation_id != 1:
            print("   -> Configuração não suportada teoricamente pelo método (rejeitada).")
            row = SummaryRow(
                method_id=method_id,
                method_name=method_name,
                situation_id=situation_id,
                situation_name=situation_name,
                regime=regime,
                quantity=quantity,
                prep_p50_ms=None,
                prep_p75_ms=None,
                prep_p99_ms=None,
                prep_reads_p50=None,
                prep_comps_p50=None,
                search_p50_ms=None,
                search_p75_ms=None,
                search_p99_ms=None,
                search_reads_p50=None,
                search_comps_p50=None,
                valid_runs=0,
                status="UNSUPPORTED",
            )
            self.record_summary_row(row)
            return row

        # Para Warm Start, garante que o cache correspondente ao N corrente já existe
        if regime.lower() == "warm":
            # Remove qualquer cache de N anterior para não validar cache menor
            self.remove_caches(method_id, situation_id)
            warmup_key = self.sample_key(situation_id, quantity)
            try:
                ret, out, err, _ = self.run_executable(method_id, quantity, situation_id, warmup_key)
                status, _ = self.parse_output(ret, out, err)
                if status in ("UNSUPPORTED", "NOT_IMPLEMENTED"):
                    print(f"   -> Executável reportou status: {status}")
                    row = SummaryRow(
                        method_id=method_id,
                        method_name=method_name,
                        situation_id=situation_id,
                        situation_name=situation_name,
                        regime=regime,
                        quantity=quantity,
                        prep_p50_ms=None,
                        prep_p75_ms=None,
                        prep_p99_ms=None,
                        prep_reads_p50=None,
                        prep_comps_p50=None,
                        search_p50_ms=None,
                        search_p75_ms=None,
                        search_p99_ms=None,
                        search_reads_p50=None,
                        search_comps_p50=None,
                        valid_runs=0,
                        status=status,
                    )
                    self.record_summary_row(row)
                    return row
            except subprocess.TimeoutExpired:
                print("   -> Timeout durante a construção de aquecimento inicial (warmup).")
                row = SummaryRow(
                    method_id=method_id,
                    method_name=method_name,
                    situation_id=situation_id,
                    situation_name=situation_name,
                    regime=regime,
                    quantity=quantity,
                    prep_p50_ms=None,
                    prep_p75_ms=None,
                    prep_p99_ms=None,
                    prep_reads_p50=None,
                    prep_comps_p50=None,
                    search_p50_ms=None,
                    search_p75_ms=None,
                    search_p99_ms=None,
                    search_reads_p50=None,
                    search_comps_p50=None,
                    valid_runs=0,
                    status="TIMEOUT",
                )
                self.record_summary_row(row)
                return row

        valid_runs: List[SingleRunResult] = []
        max_attempts = self.runs_per_config * 3  # Margem para descartar chaves não encontradas
        attempt = 0

        while len(valid_runs) < self.runs_per_config and attempt < max_attempts:
            attempt += 1

            # No regime Cold Start, remove o cache antes de CADA execução individual
            if regime.lower() == "cold":
                self.remove_caches(method_id, situation_id)

            key = self.sample_key(situation_id, quantity)

            try:
                ret, out, err, wall_t = self.run_executable(method_id, quantity, situation_id, key)
                status, metrics = self.parse_output(ret, out, err)

                if status == "UNSUPPORTED":
                    print("   -> Não suportado pelo executável.")
                    row = SummaryRow(
                        method_id=method_id,
                        method_name=method_name,
                        situation_id=situation_id,
                        situation_name=situation_name,
                        regime=regime,
                        quantity=quantity,
                        prep_p50_ms=None,
                        prep_p75_ms=None,
                        prep_p99_ms=None,
                        prep_reads_p50=None,
                        prep_comps_p50=None,
                        search_p50_ms=None,
                        search_p75_ms=None,
                        search_p99_ms=None,
                        search_reads_p50=None,
                        search_comps_p50=None,
                        valid_runs=0,
                        status="UNSUPPORTED",
                    )
                    self.record_summary_row(row)
                    return row

                if status == "NOT_IMPLEMENTED":
                    print("   -> Método ainda não implementado no binário.")
                    row = SummaryRow(
                        method_id=method_id,
                        method_name=method_name,
                        situation_id=situation_id,
                        situation_name=situation_name,
                        regime=regime,
                        quantity=quantity,
                        prep_p50_ms=None,
                        prep_p75_ms=None,
                        prep_p99_ms=None,
                        prep_reads_p50=None,
                        prep_comps_p50=None,
                        search_p50_ms=None,
                        search_p75_ms=None,
                        search_p99_ms=None,
                        search_reads_p50=None,
                        search_comps_p50=None,
                        valid_runs=0,
                        status="NOT_IMPLEMENTED",
                    )
                    self.record_summary_row(row)
                    return row

                if status == "NOT_FOUND":
                    # Regra metodológica 2: Chave ausente é sumariamente desconsiderada e reamostrada
                    if self.verbose:
                        print(f"   [Tentativa {attempt}] Chave {key} não encontrada. Desconsiderando e reamostrando...")
                    continue

                if status == "ERROR":
                    print(f"   [Erro] Falha na execução com chave {key}: {metrics.get('error')}")
                    continue

                # Sucesso: chave encontrada e métricas extraídas
                run_res = SingleRunResult(
                    method_id=method_id,
                    method_name=method_name,
                    situation_id=situation_id,
                    situation_name=situation_name,
                    quantity=quantity,
                    regime=regime,
                    run_index=len(valid_runs) + 1,
                    key=key,
                    prep_time_ms=metrics["prep_time_ms"],
                    prep_reads=metrics["prep_reads"],
                    prep_comps=metrics["prep_comps"],
                    search_time_ms=metrics["search_time_ms"],
                    search_reads=metrics["search_reads"],
                    search_comps=metrics["search_comps"],
                    status="OK",
                )
                valid_runs.append(run_res)
                self.record_raw_result(run_res)

                if len(valid_runs) % 20 == 0 or len(valid_runs) == self.runs_per_config:
                    print(f"   Progresso: {len(valid_runs)}/{self.runs_per_config} execuções válidas concluídas.")

            except subprocess.TimeoutExpired:
                print(f"   [Timeout] Execução excedeu o limite de {self.timeout_seconds:.1f}s.")
                timeout_res = SingleRunResult(
                    method_id=method_id,
                    method_name=method_name,
                    situation_id=situation_id,
                    situation_name=situation_name,
                    quantity=quantity,
                    regime=regime,
                    run_index=len(valid_runs) + 1,
                    key=key,
                    prep_time_ms=self.timeout_seconds * 1000.0,
                    prep_reads=0,
                    prep_comps=0,
                    search_time_ms=0.0,
                    search_reads=0,
                    search_comps=0,
                    status="TIMEOUT",
                )
                self.record_raw_result(timeout_res)

                if self.skip_on_timeout:
                    print("   -> Interrompendo configuração corrente devido a timeout.")
                    row = SummaryRow(
                        method_id=method_id,
                        method_name=method_name,
                        situation_id=situation_id,
                        situation_name=situation_name,
                        regime=regime,
                        quantity=quantity,
                        prep_p50_ms=None,
                        prep_p75_ms=None,
                        prep_p99_ms=None,
                        prep_reads_p50=None,
                        prep_comps_p50=None,
                        search_p50_ms=None,
                        search_p75_ms=None,
                        search_p99_ms=None,
                        search_reads_p50=None,
                        search_comps_p50=None,
                        valid_runs=len(valid_runs),
                        status="TIMEOUT",
                    )
                    self.record_summary_row(row)
                    return row

        if not valid_runs:
            print("   -> Nenhuma execução válida obtida.")
            row = SummaryRow(
                method_id=method_id,
                method_name=method_name,
                situation_id=situation_id,
                situation_name=situation_name,
                regime=regime,
                quantity=quantity,
                prep_p50_ms=None,
                prep_p75_ms=None,
                prep_p99_ms=None,
                prep_reads_p50=None,
                prep_comps_p50=None,
                search_p50_ms=None,
                search_p75_ms=None,
                search_p99_ms=None,
                search_reads_p50=None,
                search_comps_p50=None,
                valid_runs=0,
                status="FAILED",
            )
            self.record_summary_row(row)
            return row

        # Consolidação estatística dos percentis
        prep_times = [r.prep_time_ms for r in valid_runs]
        search_times = [r.search_time_ms for r in valid_runs]
        prep_reads = [r.prep_reads for r in valid_runs]
        prep_comps = [r.prep_comps for r in valid_runs]
        search_reads = [r.search_reads for r in valid_runs]
        search_comps = [r.search_comps for r in valid_runs]

        prep_p50, prep_p75, prep_p99 = compute_percentiles(prep_times)
        search_p50, search_p75, search_p99 = compute_percentiles(search_times)

        prep_reads_p50 = compute_median_int(prep_reads)
        prep_comps_p50 = compute_median_int(prep_comps)
        search_reads_p50 = compute_median_int(search_reads)
        search_comps_p50 = compute_median_int(search_comps)

        summary_row = SummaryRow(
            method_id=method_id,
            method_name=method_name,
            situation_id=situation_id,
            situation_name=situation_name,
            regime=regime,
            quantity=quantity,
            prep_p50_ms=prep_p50,
            prep_p75_ms=prep_p75,
            prep_p99_ms=prep_p99,
            prep_reads_p50=prep_reads_p50,
            prep_comps_p50=prep_comps_p50,
            search_p50_ms=search_p50,
            search_p75_ms=search_p75,
            search_p99_ms=search_p99,
            search_reads_p50=search_reads_p50,
            search_comps_p50=search_comps_p50,
            valid_runs=len(valid_runs),
            status="OK",
        )
        self.record_summary_row(summary_row)

        print(
            f"   [Consolidado] Prep: p50={prep_p50:.2f}ms, p75={prep_p75:.2f}ms, p99={prep_p99:.2f}ms | "
            f"Busca: p50={search_p50:.2f}ms, p75={search_p75:.2f}ms, p99={search_p99:.2f}ms | "
            f"Leituras={search_reads_p50}, Comps={search_comps_p50}"
        )
        return summary_row

    def run_all(self) -> None:
        """Executa toda a grade experimental planejada."""
        print("=" * 70)
        print("INICIANDO SUITE DE BENCHMARKS -- ESTRUTURA DE DADOS II (BCC203)")
        print(f"Executável: {self.executable_path}")
        print(f"Diretório de dados: {self.tmp_dir}")
        print(f"Diretório de resultados: {self.output_dir}")
        print(f"Repetições por configuração: {self.runs_per_config}")
        print(f"Métodos: {[METHOD_NAMES[m] for m in self.methods]}")
        print(f"Situações: {[SITUATION_NAMES[s] for s in self.situations]}")
        print(f"Volumes N: {self.quantities}")
        print(f"Regimes: {self.regimes}")
        print("=" * 70)

        t_start = time.perf_counter()

        for method_id in self.methods:
            for situation_id in self.situations:
                # Otimização: Se ISA e situação != 1, já marca e segue
                if method_id == 1 and situation_id != 1:
                    for regime in self.regimes:
                        for q in self.quantities:
                            self.run_benchmark_configuration(method_id, situation_id, q, regime)
                    continue

                for regime in self.regimes:
                    for q in self.quantities:
                        self.run_benchmark_configuration(method_id, situation_id, q, regime)

        t_total = time.perf_counter() - t_start
        print("\n" + "=" * 70)
        print(f"BENCHMARKS CONCLUÍDOS COM SUCESSO EM {t_total:.2f}s!")
        print(f"CSV de Execuções Brutas: {self.raw_csv_path}")
        print(f"CSV de Sumário (Tabelas): {self.summary_csv_path}")
        print("=" * 70)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Automação experimental e benchmark para BCC203 em conformidade com o relatório.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-e",
        "--executable",
        type=Path,
        default=Path("build/pesquisa"),
        help="Caminho do binário de pesquisa compilado.",
    )
    parser.add_argument(
        "-d",
        "--tmp-dir",
        type=Path,
        default=Path("tmp"),
        help="Diretório onde estão os arquivos de dados gerados (.bin).",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path("results"),
        help="Diretório onde os arquivos CSV serão salvos.",
    )
    parser.add_argument(
        "-m",
        "--methods",
        type=str,
        default="1,2,3,4",
        help="Lista de IDs de métodos separados por vírgula (1: ISA, 2: Árvore Binária, 3: Árvore B, 4: Árvore B*).",
    )
    parser.add_argument(
        "-s",
        "--situations",
        type=str,
        default="1,2,3",
        help="Lista de situações separadas por vírgula (1: Ascendente, 2: Descendente, 3: Desordenado).",
    )
    parser.add_argument(
        "-q",
        "--quantities",
        type=str,
        default="20,2000,20000,200000,2000000",
        help="Lista de quantidades N separadas por vírgula.",
    )
    parser.add_argument(
        "-r",
        "--regimes",
        type=str,
        default="cold,warm",
        help="Lista de regimes separados por vírgula (cold, warm).",
    )
    parser.add_argument(
        "-n",
        "--runs",
        type=int,
        default=100,
        help="Quantidade de execuções válidas por configuração sob as mesmas condições.",
    )
    parser.add_argument(
        "-t",
        "--timeout",
        type=float,
        default=60.0,
        help="Tempo limite máximo (segundos) para cada execução individual.",
    )
    parser.add_argument(
        "--no-skip-on-timeout",
        action="store_true",
        help="Não interrompe a configuração corrente mesmo após timeout.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Semente do gerador aleatório para reprodutibilidade da seleção de chaves.",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Habilita mensagens detalhadas de depuração.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_arguments()

    methods = [int(x.strip()) for x in args.methods.split(",") if x.strip()]
    situations = [int(x.strip()) for x in args.situations.split(",") if x.strip()]
    quantities = [int(x.strip()) for x in args.quantities.split(",") if x.strip()]
    regimes = [x.strip().lower() for x in args.regimes.split(",") if x.strip()]

    if not args.executable.exists():
        print(f"Erro: Executável '{args.executable}' não encontrado. Compile o projeto antes de executar.", file=sys.stderr)
        sys.exit(1)

    suite = BenchmarkSuite(
        executable_path=args.executable,
        tmp_dir=args.tmp_dir,
        output_dir=args.output_dir,
        methods=methods,
        situations=situations,
        quantities=quantities,
        regimes=regimes,
        runs_per_config=args.runs,
        timeout_seconds=args.timeout,
        skip_on_timeout=not args.no_skip_on_timeout,
        seed=args.seed,
        verbose=args.verbose,
    )
    suite.run_all()


if __name__ == "__main__":
    main()
