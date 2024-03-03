#ifndef IX_MANAGER_H_H
#define IX_MANAGER_H_H

#include "RM_Manager.h"
#include "PF_Manager.h"

#define PTR_SIZE sizeof(RID)

typedef struct {
	int attrLength;				//��������������ֵ�ĳ���
	int keyLength;
	AttrType attrType;			//��������������ֵ������
	PageNum rootPage;			//B+�����ڵ��ҳ���
	PageNum first_leaf;	 		//B+����һ��Ҷ�ӽڵ��ҳ���
	int order;					//B+��������
}IX_FileHeader;

typedef struct {
	bool bOpen;				//����������Ƿ��Ѿ���һ���ļ�����
	int fileID;		//�������ļ���Ӧ��ҳ���ļ����
	IX_FileHeader fileHeader;	//�������ļ��Ŀ���ҳ���
}IX_IndexHandle;

typedef struct {
	int is_leaf;          	 	//�ýڵ��Ƿ�ΪҶ�ӽڵ�
	int keynum;				//�ýڵ�ʵ�ʰ����Ĺؼ��ָ���
	PageNum parent;     		//ָ�򸸽ڵ����ڵ�ҳ���
	PageNum brother;     		//ָ�����ֵܽڵ����ڵ�ҳ���
	char* keys;					//ָ��ؼ�������ָ��
	RID* rids;					//ָ��ָ������ָ��
}IX_Node;

typedef struct {
	bool bOpen;						//ɨ���Ƿ��
	IX_IndexHandle* pIXIndexHandle;	//ָ�������ļ������ָ��
	CompOp compOp;  				//���ڱȽϵĲ�����
	char* value;		 				//������ֵ���бȽϵ�ֵ
	PF_PageHandle  PageHandle;		//�����е�ҳ����
	PageNum  pn; 	//ɨ�輴�������ҳ���
	int  ridIx;			//ɨ�輴���������������
}IX_IndexScan;

typedef struct Tree_Node{
	int  keyNum;		//�ڵ��а����Ĺؼ��֣�����ֵ������
	char  **keys;		//�ڵ��а����Ĺؼ��֣�����ֵ������
	Tree_Node  *parent;	//���ڵ�
	Tree_Node  *sibling;	//�ұߵ��ֵܽڵ�
	Tree_Node  *firstChild;	//����ߵĺ��ӽڵ�
}Tree_Node; //�ڵ����ݽṹ

typedef struct{
	AttrType  attrType;	//B+����Ӧ���Ե���������
	int  attrLength;	//B+����Ӧ����ֵ�ĳ���
	int  order;			//B+��������
	Tree_Node  *root;	//B+���ĸ��ڵ�
}Tree;

//�˺�������һ����ΪfileName��������attrType�������������Ե����ͣ�attrLength�������������Եĳ��ȡ�
RC CreateIndex(const char * fileName,AttrType attrType,int attrLength);

//�˺�������ΪfileName�������ļ�������������óɹ�����indexHandleΪָ�򱻴򿪵����������ָ�롣������������������в����ɾ�������Ҳ������������ɨ�衣
RC OpenIndex(const char *fileName,IX_IndexHandle *indexHandle);

//�˺����رվ��indexHandle��Ӧ�������ļ���
RC CloseIndex(IX_IndexHandle *indexHandle);

//�˺�����IX_IndexHandle��Ӧ�������в���һ�����������pDataָ��Ҫ���������ֵ������rid��ʶ���������Ӧ��Ԫ�飬���������в���һ��ֵΪ��*pData��rid���ļ�ֵ�ԡ�
RC InsertEntry(IX_IndexHandle *indexHandle,void *pData,const RID * rid);

//�˺�����IX_IndexHandle�����Ӧ��������ɾ��һ��ֵΪ��*pData��rid���������
RC DeleteEntry(IX_IndexHandle *indexHandle,void *pData,const RID * rid);

//�˺���������indexHandle��Ӧ�������ϳ�ʼ��һ������������ɨ�衣compOp��*valueָ���ȽϷ��ͱȽ�ֵ��indexScanΪ��ʼ���������ɨ��ṹָ�롣
RC OpenIndexScan(IX_IndexScan *indexScan,IX_IndexHandle *indexHandle,CompOp compOp,char *value);

//�˺������ڼ���IX_IndexScan�����Ӧ������ɨ�裬�����һ����������������������ظ��������Ӧ�ļ�¼��ID��
RC IX_GetNextEntry(IX_IndexScan *indexScan,RID * rid);

//�ر�һ������ɨ�裬�ͷ���Ӧ����Դ��
RC CloseIndexScan(IX_IndexScan *indexScan);

//��ȡ��fileNameָ����B+���������ݣ�����ָ��B+����ָ�롣�˺����ṩ�����Գ�����ã����ڼ��B+���������ݵ���ȷ�ԡ�
RC GetIndexTree(char *fileName, Tree *index);

//������Ϣ�ṹ
struct IndexInfo {
	AttrType attrType;
	int attrLength;
	//�������������������Ϣ
	IndexInfo(IX_IndexHandle* indexHandle) : attrType(indexHandle->fileHeader.attrType), attrLength(indexHandle->fileHeader.attrLength) {
	}
};

//����ҳ������ȡҳ���е�IX_Node�ڵ���Ϣ
IX_Node* getIXNodefromPH(const PF_PageHandle* pageHandle);

//����attryypeָ�������ͱȽ����������ֵ
int CompareValue(AttrType attrType, char* keyval, char* value);

//����B+��������ֵ������value��Ҷ�ӽڵ㣬��������������ҳ���ҳ������ҳ���
RC FindFirstIXNode(IX_IndexHandle* indexHandle, char* value, PF_PageHandle* pageHandle);

//��������ֵ��RID��ֵ��ȷ��������������ڵ�ҳ�棬����ҳ�����;���λ��
RC FindIXNodeInRid(IX_IndexHandle* indexHandle, char* keyval, const RID* ridval, PF_PageHandle* pageHandle);

//��������ֵ��ҳ�Ų������ݵ������Ҷ�ӽڵ���
RC InsertEntryInLeaf(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn);

//��������ֵ��ҳ�Ų������ݵ�����ķ�Ҷ�ӽڵ���
RC InsertEntryInParent(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn);

//��������ֵ��ҳ��ɾ���ڵ��е�����
RC DeleteNode(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn);

#endif