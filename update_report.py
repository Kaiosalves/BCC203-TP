#!/usr/bin/env python3
"""
update_report.py - Utilitário de sincronização entre o benchmark e o relatório LaTeX.

Este script lê os dados consolidados em `results/benchmark_summary.csv`
e atualiza diretamente as tabelas experimentais correspondentes no arquivo `relatorio.tex`:
- Tabela 2 (label: tab:bench_isa) -> Acesso Sequencial Indexado (ISA)
- Tabela 3 (label: tab:bench_binary_tree) -> Árvore Binária de Pesquisa Externa
- Tabela 4 (label: tab:bench_btree) -> Árvore B
- Tabela 5 (label: tab:bench_bstartree) -> Árvore B*
"""

from __future__ import annotations

import argparse
import csv
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, Optional, Tuple

TABLE_LABELS: Dict[int, str] = {
    1: "tab:bench_isa",
    2: "tab:bench_binary_tree",
    3: "tab:bench_btree",
    4: "tab:bench_bstartree",
}

SITUATION_MAPPING: Dict[str, str] = {
    "1": "Ascendente",
    "ascendente": "Ascendente",
    "2": "Descendente",
    "descendente": "Descendente",
    "3": "Desordenado",
    "desordenado": "Desordenado",
}

REGIME_MAPPING: Dict[str, str] = {
    "cold": "Cold",
    "warm": "Warm",
}


def format_number_pt(val: int | float) -> str:
    """Formata inteiros com separador de milhar brasileiro (ponto)."""
    return f"{int(val):,}".replace(",", ".")


def format_time_pt(val: Optional[float]) -> str:
    """Formata valores de tempo em milissegundos com duas casas decimais."""
    if val is None:
        return "-"
    if 0.0 < val < 0.005:
        return "< 0.01"
    return f"{val:.2f}"


def format_count_pt(val: Optional[int]) -> str:
    """Formata contagens de comparações e leituras."""
    if val is None:
        return "-"
    return format_number_pt(val)


class ReportUpdater:
    def __init__(
        self,
        summary_csv_path: Path,
        tex_path: Path,
        backup: bool = True,
        dry_run: bool = False,
    ):
        self.summary_csv_path = summary_csv_path.resolve()
        self.tex_path = tex_path.resolve()
        self.backup = backup
        self.dry_run = dry_run

        # Chave: (method_id, situation_name, regime_name, quantity_int)
        self.data: Dict[Tuple[int, str, str, int], Dict[str, any]] = {}
        self._load_csv()

    def _load_csv(self) -> None:
        """Carrega e indexa os resultados do CSV sumário."""
        if not self.summary_csv_path.exists():
            print(f"[Erro] Arquivo CSV não encontrado: {self.summary_csv_path}", file=sys.stderr)
            return

        with open(self.summary_csv_path, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    m_id = int(row["method_id"])
                    sit_name = row["situation_name"].strip()
                    regime_name = REGIME_MAPPING.get(row["regime"].strip().lower(), row["regime"].strip())
                    qty = int(row["quantity"].strip())
                    status = row.get("status", "OK").strip()

                    entry = {
                        "status": status,
                        "prep_p50_ms": float(row["prep_p50_ms"]) if row["prep_p50_ms"] else None,
                        "prep_p75_ms": float(row["prep_p75_ms"]) if row["prep_p75_ms"] else None,
                        "prep_p99_ms": float(row["prep_p99_ms"]) if row["prep_p99_ms"] else None,
                        "prep_reads_p50": int(row["prep_reads_p50"]) if row["prep_reads_p50"] else None,
                        "prep_comps_p50": int(row["prep_comps_p50"]) if row["prep_comps_p50"] else None,
                        "search_p50_ms": float(row["search_p50_ms"]) if row["search_p50_ms"] else None,
                        "search_p75_ms": float(row["search_p75_ms"]) if row["search_p75_ms"] else None,
                        "search_p99_ms": float(row["search_p99_ms"]) if row["search_p99_ms"] else None,
                        "search_reads_p50": int(row["search_reads_p50"]) if row["search_reads_p50"] else None,
                        "search_comps_p50": int(row["search_comps_p50"]) if row["search_comps_p50"] else None,
                        "valid_runs": int(row["valid_runs"]) if row.get("valid_runs") else 0,
                    }
                    self.data[(m_id, sit_name, regime_name, qty)] = entry
                except (ValueError, KeyError) as e:
                    print(f"[Aviso] Linha inválida ignorada no CSV: {row} ({e})", file=sys.stderr)

        print(f"Carregadas {len(self.data)} entradas consolidadas de {self.summary_csv_path.name}.")

    def update_latex(self) -> int:
        """Percorre as tabelas de relatorio.tex e substitui as linhas com dados disponíveis."""
        if not self.tex_path.exists():
            print(f"[Erro] Arquivo LaTeX não encontrado: {self.tex_path}", file=sys.stderr)
            return 0

        content = self.tex_path.read_text(encoding="utf-8")
        updated_content = content
        total_replacements = 0

        # Expressão regular para casar linhas de dados da tabela
        # Ex: "Ascendente & Cold & 2.000      & 0.00 & 0.00 & 0.00 & 0 & 0 & 0.00 & 0.00 & 0.00 & 0 & 0 \\"
        row_regex = re.compile(
            r"^([ \t]*)(Ascendente|Descendente|Desordenado)([ \t]*&[ \t]*)(Cold|Warm)([ \t]*&[ \t]*)([0-9.]+)([ \t]*&)[^\\]*(\\\\.*)$",
            re.MULTILINE,
        )

        for m_id, label in TABLE_LABELS.items():
            label_pattern = f"\\label{{{label}}}"
            pos = updated_content.find(label_pattern)
            if pos == -1:
                print(f"[Aviso] Tabela com label '{label}' não encontrada no LaTeX.", file=sys.stderr)
                continue

            end_table = updated_content.find("\\end{table}", pos)
            if end_table == -1:
                end_table = len(updated_content)

            table_block = updated_content[pos:end_table]
            table_replacements = 0

            def replace_row(match: re.Match) -> str:
                nonlocal table_replacements, total_replacements
                indent = match.group(1)
                sit = match.group(2)
                sep1 = match.group(3)
                reg = match.group(4)
                sep2 = match.group(5)
                raw_qty = match.group(6)
                sep3 = match.group(7)
                tail = match.group(8)

                qty = int(raw_qty.replace(".", "").strip())
                key = (m_id, sit, reg, qty)

                if key not in self.data:
                    # Mantém linha original se não houver dados no CSV
                    return match.group(0)

                d = self.data[key]
                status = d["status"]

                if status == "TIMEOUT":
                    # Formata linha com indicação de timeout
                    table_replacements += 1
                    total_replacements += 1
                    values_str = (
                        f" - & - & - & - & - & - & - & - & - & - % (Timeout {qty})"
                    )
                    return f"{indent}{sit}{sep1}{reg}{sep2}{raw_qty}{sep3} {values_str} {tail}"

                if status == "UNSUPPORTED":
                    # Já possui tratamento próprio ou multicolumn no LaTeX
                    return match.group(0)

                if status != "OK":
                    return match.group(0)

                # Formata os 10 campos experimentais
                t_prep_p50 = format_time_pt(d["prep_p50_ms"])
                t_prep_p75 = format_time_pt(d["prep_p75_ms"])
                t_prep_p99 = format_time_pt(d["prep_p99_ms"])
                r_prep = format_count_pt(d["prep_reads_p50"])
                c_prep = format_count_pt(d["prep_comps_p50"])

                t_search_p50 = format_time_pt(d["search_p50_ms"])
                t_search_p75 = format_time_pt(d["search_p75_ms"])
                t_search_p99 = format_time_pt(d["search_p99_ms"])
                r_search = format_count_pt(d["search_reads_p50"])
                c_search = format_count_pt(d["search_comps_p50"])

                # Alinhamento estético
                values_str = (
                    f"{t_prep_p50:>7} & {t_prep_p75:>7} & {t_prep_p99:>7} & "
                    f"{r_prep:>5} & {c_prep:>5} & "
                    f"{t_search_p50:>7} & {t_search_p75:>7} & {t_search_p99:>7} & "
                    f"{r_search:>5} & {c_search:>5}"
                )

                table_replacements += 1
                total_replacements += 1
                return f"{indent}{sit}{sep1}{reg}{sep2}{raw_qty:<10}{sep3} {values_str} {tail}"

            new_table_block = row_regex.sub(replace_row, table_block)
            updated_content = (
                updated_content[:pos] + new_table_block + updated_content[end_table:]
            )
            print(f"Tabela '{label}' (Método {m_id}): {table_replacements} linhas atualizadas.")

        if total_replacements > 0:
            if self.dry_run:
                print(f"[Dry-Run] Simulação concluída: {total_replacements} linhas seriam atualizadas.")
            else:
                if self.backup:
                    backup_path = self.tex_path.with_suffix(".tex.bak")
                    shutil.copyfile(self.tex_path, backup_path)
                    print(f"Backup criado em: {backup_path}")

                self.tex_path.write_text(updated_content, encoding="utf-8")
                print(f"Arquivo LaTeX '{self.tex_path}' atualizado com sucesso ({total_replacements} alterações).")
        else:
            print("Nenhuma linha necessitou de atualização com os dados fornecidos.")

        return total_replacements

    def compile_pdf(self) -> bool:
        """Invoca o pdflatex para gerar o PDF a partir do LaTeX atualizado."""
        print("\nCompilando documento PDF com pdflatex...")
        try:
            res = subprocess.run(
                ["pdflatex", "-interaction=nonstopmode", self.tex_path.name],
                cwd=str(self.tex_path.parent),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            if res.returncode == 0:
                print(f"PDF compilado com sucesso: {self.tex_path.with_suffix('.pdf')}")
                return True
            else:
                print("[Aviso] pdflatex retornou avisos ou erros na compilação.", file=sys.stderr)
                # Verifica se o PDF foi gerado mesmo assim
                pdf_path = self.tex_path.with_suffix(".pdf")
                if pdf_path.exists():
                    print(f"PDF gerado com advertências: {pdf_path}")
                    return True
                return False
        except FileNotFoundError:
            print("[Erro] Comando 'pdflatex' não encontrado no ambiente.", file=sys.stderr)
            return False


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Atualiza as tabelas experimentais do relatorio.tex a partir do benchmark_summary.csv.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-c",
        "--csv",
        type=Path,
        default=Path("results/benchmark_summary.csv"),
        help="Caminho do arquivo benchmark_summary.csv gerado pelo benchmark.",
    )
    parser.add_argument(
        "-t",
        "--tex",
        type=Path,
        default=Path("relatorio.tex"),
        help="Caminho do arquivo de relatório relatorio.tex a ser atualizado.",
    )
    parser.add_argument(
        "--no-backup",
        action="store_true",
        help="Não gera cópia de backup do arquivo .tex antes de sobrescrever.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Apenas simula e relata as linhas que seriam alteradas.",
    )
    parser.add_argument(
        "--compile",
        action="store_true",
        help="Compila o PDF automaticamente após a atualização do LaTeX.",
    )

    args = parser.parse_args()

    updater = ReportUpdater(
        summary_csv_path=args.csv,
        tex_path=args.tex,
        backup=not args.no_backup,
        dry_run=args.dry_run,
    )

    reps = updater.update_latex()
    if reps > 0 and args.compile and not args.dry_run:
        updater.compile_pdf()


if __name__ == "__main__":
    main()
