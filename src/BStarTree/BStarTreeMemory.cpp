#include "BStarTreeMemory.hpp"

namespace Algorithm::BStarTree{

BStarTreeMemory::BStarTreeMemory() : rootIndex_(-1){
//forca os nos comecarem zerados
    this->nodes_.clear();
}


BStarTreeMemory::BStarTreeMemory(File& input) : rootIndex_(-1){
//chama a funcao para povoar a arvore
    this->PopulateTree(input);
}

uint64_t BStarTreeMemory::AppendNode(const Node& node){
//insere o no no final do vetor    
    this->nodes_.push_back(node);

//retorna o indice do no
    return static_cast<uint64_t>(this->nodes_.size() -1);
}
void BStarTreeMemory::InsertPage(const std::array<Item, PAGE_SIZE>& page, uint64_t pageIndex,size_t itemCount){

//percorre os itens da pagina
    for(size_t i = 0; i < itemCount; i++){
//insere o item na arvore
        this->InsertItem(page[i], pageIndex);
    }
}


void BStarTreeMemory::PopulateTree(File& input){
    uint64_t pageIndex = 0;

//varre o arquivo ate o final
    while(!input.eof()){
        std::array<Item, PAGE_SIZE> page = input.GetNextPage();

//insere a pagina, que insere os itens na arvore na memoria
        this->InsertPage(page, pageIndex, PAGE_SIZE);
        pageIndex++;
    }
}


}