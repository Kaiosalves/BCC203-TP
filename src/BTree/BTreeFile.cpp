#include "BTreeFile.hpp"

#include <stdexcept>

#include "../Common.hpp"


using namespace Algorithm::BTree;

BTreeFile::BTreeFile(File& input){
    std::string const path = GetFilePath(input); 

    if (!TryLoadExistingFile(path, input)) {
        BuildFile(input, path);
    }
}

BTreeFile::~BTreeFile(){
    if (this->file_.is_open()) {
        this->file_.close();
    }
}
bool BTreeFile::Node::isLeaf()const{
    for(auto& pos : nodePos){
        if(pos != -1){
            return false;
        }
    }
    return true;
}


BTreeFile::Node::Node() : size(0){
//preenche todos os nos filhos incialmente com -1 indicando vazio
    nodePos.fill(-1);
}



std::string BTreeFile::GetFilePath(const File& input){
    return input.path()+ ".btree";
}

void BTreeFile::WriteMetadata(std::fstream& file, const File& input,int64_t rootIndex){
    // Reiniciando arquivo
    file.clear();
    file.seekg(0, std::ios::beg);
    file.seekp(0, std::ios::beg);
    // Buscando  metadados
    Metadata tmp;
    tmp.lastModification = input.lastModification();
    tmp.size = input.size();
    // escrevendo os metadados na arvore
    file.write(reinterpret_cast<char*>(&tmp), sizeof(Metadata));
}
std::streamoff BTreeFile::GetNodeOffset(uint64_t nodeIndex){
    return static_cast<std::streamoff>((nodeIndex * sizeof(Node)) +
                                       sizeof(Metadata));

}

bool BTreeFile::TryLoadExistingFile(const std::string& path, const File& input){
    this->file_.open(path, std::ios::in | std::ios::binary);
    
    if (!this->file_.is_open()) {
        return false;
    }

    if (!ValidateFile(input)) {
        this->file_.close();
        return false;
}

return true;
}

bool BTreeFile::ValidateFile(const File& input){
    //reposiciona o ponteiro pro inicio do arquivo de cache
    this->file_.seekg(0,std::ifstream::beg);

//le e armazena os metadados do disco(tamanho e lastmodification)
    Metadata m;

    if(!this->file_.read((char*)&m, sizeof(Metadata))){
    this->file_.clear();
    return false;
    }
//variaveis que verificam se o cache tem os mesmos metadados
    bool sameModificationTime = (m.lastModification == input.lastModification());

    bool sameSize = (m.size == input.size());

//retorna verdadeiro se eles tem os mesmos metadados, falso se nao
    return sameModificationTime && sameSize;
}

void BTreeFile::WriteNode(std::fstream& file, uint64_t nodeIndex,const Node& node){
    file.seekp(GetNodeOffset(nodeIndex));
    file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
}

bool BTreeFile::ReadNode(std::istream& file, uint64_t nodeIndex, Node& node){
       
    std::streamoff const offset = GetNodeOffset(nodeIndex);

    file.seekg(offset, std::ios::beg);
        
    file.read(reinterpret_cast<char*>(&node), sizeof(Node));
    
    if(file.fail()){
        return false;
    }

    return true;
}

uint64_t BTreeFile::AppendNode(std::fstream& file, const Node& node,uint64_t& lastNodeIndex){
    lastNodeIndex++;  
    
    WriteNode(file, lastNodeIndex, node);


    return lastNodeIndex;
}




