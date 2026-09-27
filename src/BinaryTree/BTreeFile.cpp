#include "BTreeFile.hpp"

#include "Common.hpp"
//#include "File.hpp"
//#include "Item.hpp"

namespace Algorithm::BinaryTree{
    std::streamoff BTreeFile::GetNodeOffset(uint64_t nodeIndex) {
        return static_cast<std::streamoff>(nodeIndex * sizeof(Node) + sizeof(Metadata));
    }
    void BTreeFile::WriteNode(std::fstream& file, uint64_t nodeIndex,const Node& node){
        file.seekg(GetNodeOffset(nodeIndex));
        file.write(reinterpret_cast<const char*>(&node),sizeof(Node));
    }
    bool BTreeFile::ReadNode(std::istream& file, uint64_t nodeIndex, Node& node){
        if(!file.eof()){
            file.seekg(GetNodeOffset(nodeIndex));
            if(file.eof())
                return false;
            file.read(reinterpret_cast<char*>(&node), sizeof(Node));
            return true;
        }
        return false;
    }
    uint64_t BTreeFile::AppendNode(std::fstream& file, const Node& node,
                               uint64_t& lastNodeIndex){
        lastNodeIndex++;//incrementa o contador
        file.seekg(GetNodeOffset(lastNodeIndex));
        if(!file.eof()){
            WriteNode(file, lastNodeIndex,node);
            return lastNodeIndex;
        }
        return 0;
    }
    void BTreeFile::WriteMetadata(std::fstream& file, const File& input){
        //Reiniciando arquivo
        file.clear();
        file.seekg(0, std::ios::beg);
        file.seekp(0, std::ios::beg);
        //Buscando  metadados
        Metadata tmp;
        tmp.lastModification = input.lastModification();
        tmp.size = input.size();
        //escrevendo os metadados na arvore
        file.write(reinterpret_cast<char*>(&tmp), sizeof(Metadata));
    }
    void BTreeFile::InitializeRoot(std::fstream& file, File& input){
        //calcula onde a página central está
        uint64_t itemMeio = input.quantity() / 2;
        u_int64_t pageIndex = itemMeio / PAGE_SIZE;
        
        u_int64_t offsetNaPag = itemMeio % PAGE_SIZE;//item central da página do meio
        std::array<Item, PAGE_SIZE> page;
        page = input.GetPageAt(pageIndex);
        //inicializando valores
        Node noRaiz;
        noRaiz.key = page[offsetNaPag].key;
        noRaiz.pageIndex = pageIndex;
        noRaiz.left = 0; noRaiz.right = 0;
        //Escreve no final do aquivo
        WriteNode(file, 0, noRaiz);
    }
    void BTreeFile::InsertItem(std::fstream& file, const Item& item,
                           uint64_t pageIndex, uint64_t& lastNodeIndex){
        
    }
    void BTreeFile::InsertPage(std::fstream& file,
                           const std::array<Item, PAGE_SIZE>& page,
                           uint64_t pageIndex, uint64_t& lastNodeIndex){
        for(int i = 0; i < PAGE_SIZE; i++){
            InsertItem(file, page[i], pageIndex, lastNodeIndex);
        }
    }

    void BTreeFile::PopulateTree(std::fstream& file, File& input,
                             uint64_t& lastNodeIndex){
        lastNodeIndex = 0;
        u_int64_t pageIndex = 0;
        std::array<Item, PAGE_SIZE> page;
        while(!input.eof()){
            page = input.GetNextPage();
            InsertPage(file,page, pageIndex,lastNodeIndex);
            pageIndex++;
        }
    }


};



