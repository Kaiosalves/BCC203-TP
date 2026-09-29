#pragma once

#include <optional>

#include "../File.hpp"
#include "../Item.hpp"
#include "Cache.hpp"

namespace Algorithm::IndexedSequentialAccess {
class ISA {
public:
    // 2
    /**
     * @brief Construtor da classe ISA (Indexed Sequential Access).
     *
     * Armazena o ponteiro compartilhado para o arquivo de dados (`input_`)
     * e inicializa o objeto `cache_` (tabela de índices), o qual carrega um
     * índice pré-existente ou constrói um novo índice ordenado em disco.
     *
     * @param input Ponteiro compartilhado para o arquivo binário de dados.
     */
    explicit ISA(const std::shared_ptr<File>& input);

    /**
     * @brief Destrutor padrão da classe ISA.
     *
     * Libera os recursos associados e finaliza os objetos membros `input_` e
     * `cache_`.
     */
    ~ISA() = default;

    // 2
    /**
     * @brief Executa a pesquisa de um registro pela chave usando Acesso
     * Sequencial Indexado.
     *
     * Consulta primeiramente a tabela de índice em disco através de
     * `cache_.Search(key)` para localizar em qual página do arquivo de dados a
     * chave reside:
     * - Se a chave não for encontrada no índice, encerra a busca retornando
     * vazio.
     * - Se for encontrada, recupera o `pageIndex` correspondente e carrega essa
     * página específica do arquivo de dados para a memória principal com
     * `input_->GetPageAt(...)`.
     * - Realiza a busca linear nos itens contidos na página em memória.
     *
     * @param key Chave numérica inteira a ser pesquisada.
     * @return std::optional<Item> O item correspondente caso encontrado na
     * página; caso contrário, std::nullopt (vazio).
     */
    std::optional<Item> Search(int key);

private:
    std::shared_ptr<File> input_;
    Cache cache_;
};
}  // namespace Algorithm::IndexedSequentialAccess
