#ifndef IX_MANAGER_H_H
#define IX_MANAGER_H_H

#include "RM_Manager.h"
#include "PF_Manager.h"

#define PTR_SIZE sizeof(RID)

typedef struct {
	int attrLength;				//建立索引的属性值的长度
	int keyLength;
	AttrType attrType;			//建立索引的属性值的类型
	PageNum rootPage;			//B+树根节点的页面号
	PageNum first_leaf;	 		//B+树第一个叶子节点的页面号
	int order;					//B+树的序数
}IX_FileHeader;

typedef struct {
	bool bOpen;				//该索引句柄是否已经与一个文件关联
	int fileID;		//该索引文件对应的页面文件句柄
	IX_FileHeader fileHeader;	//该索引文件的控制页句柄
}IX_IndexHandle;

typedef struct {
	int is_leaf;          	 	//该节点是否为叶子节点
	int keynum;				//该节点实际包含的关键字个数
	PageNum parent;     		//指向父节点所在的页面号
	PageNum brother;     		//指向右兄弟节点所在的页面号
	char* keys;					//指向关键字区的指针
	RID* rids;					//指向指针区的指针
}IX_Node;

typedef struct {
	bool bOpen;						//扫描是否打开
	IX_IndexHandle* pIXIndexHandle;	//指向索引文件句柄的指针
	CompOp compOp;  				//用于比较的操作符
	char* value;		 				//与属性值进行比较的值
	PF_PageHandle  PageHandle;		//处理中的页面句柄
	PageNum  pn; 	//扫描即将处理的页面号
	int  ridIx;			//扫描即将处理的索引项编号
}IX_IndexScan;

typedef struct Tree_Node{
	int  keyNum;		//节点中包含的关键字（属性值）个数
	char  **keys;		//节点中包含的关键字（属性值）数组
	Tree_Node  *parent;	//父节点
	Tree_Node  *sibling;	//右边的兄弟节点
	Tree_Node  *firstChild;	//最左边的孩子节点
}Tree_Node; //节点数据结构

typedef struct{
	AttrType  attrType;	//B+树对应属性的数据类型
	int  attrLength;	//B+树对应属性值的长度
	int  order;			//B+树的序数
	Tree_Node  *root;	//B+树的根节点
}Tree;

//此函数创建一个名为fileName的索引。attrType描述被索引属性的类型，attrLength描述被索引属性的长度。
RC CreateIndex(const char * fileName,AttrType attrType,int attrLength);

//此函数打开名为fileName的索引文件。如果方法调用成功，则indexHandle为指向被打开的索引句柄的指针。索引句柄用于在索引中插入或删除索引项，也可用于索引的扫描。
RC OpenIndex(const char *fileName,IX_IndexHandle *indexHandle);

//此函数关闭句柄indexHandle对应的索引文件。
RC CloseIndex(IX_IndexHandle *indexHandle);

//此函数向IX_IndexHandle对应的索引中插入一个索引项。参数pData指向要插入的属性值，参数rid标识该索引项对应的元组，即向索引中插入一个值为（*pData，rid）的键值对。
RC InsertEntry(IX_IndexHandle *indexHandle,void *pData,const RID * rid);

//此函数从IX_IndexHandle句柄对应的索引中删除一个值为（*pData，rid）的索引项。
RC DeleteEntry(IX_IndexHandle *indexHandle,void *pData,const RID * rid);

//此函数用于在indexHandle对应的索引上初始化一个基于条件的扫描。compOp和*value指定比较符和比较值，indexScan为初始化后的索引扫描结构指针。
RC OpenIndexScan(IX_IndexScan *indexScan,IX_IndexHandle *indexHandle,CompOp compOp,char *value);

//此函数用于继续IX_IndexScan句柄对应的索引扫描，获得下一个满足条件的索引项，并返回该索引项对应的记录的ID。
RC IX_GetNextEntry(IX_IndexScan *indexScan,RID * rid);

//关闭一个索引扫描，释放相应的资源。
RC CloseIndexScan(IX_IndexScan *indexScan);

//获取由fileName指定的B+树索引内容，返回指向B+树的指针。此函数提供给测试程序调用，用于检查B+树索引内容的正确性。
RC GetIndexTree(char *fileName, Tree *index);

//索引信息结构
struct IndexInfo {
	AttrType attrType;
	int attrLength;
	//用索引句柄构造索引信息
	IndexInfo(IX_IndexHandle* indexHandle) : attrType(indexHandle->fileHeader.attrType), attrLength(indexHandle->fileHeader.attrLength) {
	}
};

//根据页面句柄获取页面中的IX_Node节点信息
IX_Node* getIXNodefromPH(const PF_PageHandle* pageHandle);

//根据attryype指定的类型比较两块区域的值
int CompareValue(AttrType attrType, char* keyval, char* value);

//查找B+树中属性值可能有value的叶子节点，返回索引项所在页面的页面句柄、页面号
RC FindFirstIXNode(IX_IndexHandle* indexHandle, char* value, PF_PageHandle* pageHandle);

//根据属性值和RID的值精确查找索引项可能在的页面，返回页面句柄和具体位置
RC FindIXNodeInRid(IX_IndexHandle* indexHandle, char* keyval, const RID* ridval, PF_PageHandle* pageHandle);

//根据属性值和页号插入内容到具体的叶子节点中
RC InsertEntryInLeaf(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn);

//根据属性值和页号插入内容到具体的非叶子节点中
RC InsertEntryInParent(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn);

//根据属性值和页号删除节点中的内容
RC DeleteNode(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn);

#endif