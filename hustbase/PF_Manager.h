#ifndef PF_MANAGER_H_H
#define PF_MANAGER_H_H

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#include "RC.h"

#define PF_PAGE_SIZE ((1<<12)-4)
#define PF_FILESUBHDR_SIZE (sizeof(PF_FileSubHeader))
#define PF_BUFFER_SIZE 50//ҳ�滺�����Ĵ�С
#define PF_PAGESIZE (1<<12)
#define MAX_OPEN_FILE 50

typedef unsigned int PageNum;

typedef struct {
	PageNum pageNum;				//ҳ��(��0��ʼ���)
	char pData[PF_PAGE_SIZE];		//������
}Page;

typedef struct {
	PageNum pageCount;			//βҳ��ҳ��
	int nAllocatedPages;			//�ѷ���ҳ����Ŀ(��������ҳ)
}PF_FileSubHeader;

typedef struct {
	bool bDirty;		//True ��ʾҳ�������Ѿ����޸ģ�False��ʾû�б��޸�
	unsigned int pinCount; 	//���ʵ�ǰҳ����߳���
	time_t 	accTime;		//ҳ��������ʱ��
	char* fileName;			//����ҳ�����ڵ��ļ���
	int fileDesc;				//����ļ���������ļ�������
	Page page;				//��Ӳ���л�ȡ��ʵ��ҳ��
}Frame;

typedef struct {
	bool bOpen;			//���ļ�����Ƿ��Ѿ���һ���ļ�����
	char* fileName;			//��þ���������ļ���
	int  fileDesc;			//��þ���������ļ�������
	Frame* pHdrFrame;		//ָ����ļ�ͷ֡������ҳ��Ӧ��֡����ָ��
	Page* pHdrPage;		//ָ����ļ�ͷҳ������ҳ����ָ��
	char* pBitmap;		//ָ�����ҳ��λͼ��ָ��
	PF_FileSubHeader *pFileSubHeader;	//���ļ���PF_FileSubHeader�ṹ
} PF_FileHandle;
	

typedef struct __BF_Manager {
	int nReads;
	int nWrites;
	Frame frame[PF_BUFFER_SIZE];
	bool allocated[PF_BUFFER_SIZE];

	__BF_Manager()
	{
		for (int i = 0; i < PF_BUFFER_SIZE; i++) {
			allocated[i] = false;
			frame[i].pinCount = 0;
		}
	}

}BF_Manager;

typedef struct {
	bool  bOpen;			//True ��ʾҳ��������
	Frame* pFrame;			//ָ��ǰ��������ָ��
}PF_PageHandle;

//����һ������Ϊָ���ļ����ķ�ҳ�ļ���
const RC CreateFile(const char *fileName);

//�����ļ�����һ����ҳ�ļ��������ļ�ID��
const RC OpenFile(char *fileName,int * FileID);

//�ر�FileID��Ӧ�ķ�ҳ�ļ���
const RC CloseFile(int FileID);

//�����ļ�ID��ҳ�Ż�ȡָ��ҳ�浽������������ҳ����ָ�롣
const RC GetThisPage(int FileID,PageNum pageNum,PF_PageHandle *pageHandle);

//��ָ���ļ��з���һ���µ�ҳ�棬��������뻺����������ҳ����ָ�롣����ҳ��ʱ������ļ����п���ҳ����ֱ�ӷ���һ������ҳ������ļ���û�п���ҳ������չ�ļ���ģ�������µĿ���ҳ��
const RC AllocatePage(int FileID,PF_PageHandle *pageHandle);

//����ҳ����ָ�뷵�ض�Ӧ��ҳ��š�
const RC GetPageNum(PF_PageHandle *pageHandle,PageNum *pageNum);
const RC GetFileHandle(PF_FileHandle** fileHandle, int fileID);
const RC GetData(PF_PageHandle *pageHandle,char **pData);

//�����ļ��б��ΪpageNum��ҳ�棬�����Ϊ����ҳ��
const RC DisposePage(int FileID,PageNum pageNum);

//���ָ��ҳ��Ϊ���ࡱҳ������޸���ҳ������ݣ���Ӧ���ô˺������Ա��ҳ�汻��̭��������ʱϵͳ���µ�ҳ������д������ļ���
const RC MarkDirty(PF_PageHandle *pageHandle);

//�˺������ڽ��pageHandle��Ӧҳ���פ�����������ơ��ڵ���GetThisPage��AllocatePage������һ��ҳ����뻺�����󣬸�ҳ�汻����Ϊפ��������״̬���Է�ֹ���ڴ�������б��û���ȥ������ڸ�ҳ��ʹ����֮��Ӧ���ô˺�����������ƣ�ʹ�ø�ҳ��˺���������ر���̭����������
const RC UnpinPage(PF_PageHandle *pageHandle);

//��ȡ�ļ�����ҳ����
const RC GetPageCount(int FileID, int *pageCount);

#endif
