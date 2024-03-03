#include "IX_Manager.h"



//����ҳ������ȡҳ���е�IX_Node�ڵ���Ϣ
IX_Node* getIXNodefromPH(const PF_PageHandle* pageHandle) {
	return (IX_Node*)(pageHandle->pFrame->page.pData + sizeof(IX_FileHeader));
}


RC CreateIndex(const char* fileName, AttrType attrType, int attrLength)
{
	//������Ӧ��ҳ���ļ�
	RC tmp;
	if ((tmp = CreateFile(fileName)) != SUCCESS) {
		return tmp;
	}

	//�򿪴������ļ�
	int fileID;
	if ((tmp = OpenFile((char*)fileName, &fileID)) != SUCCESS) {
		return tmp;
	}

	//��ȡ��һҳ����������������Ϣ
	PF_PageHandle pageHandle;
	if ((tmp = AllocatePage(fileID, &pageHandle)) != SUCCESS) {
		return tmp;
	}

	IX_FileHeader* fileHeader = (IX_FileHeader*)pageHandle.pFrame->page.pData;
	fileHeader->attrLength = attrLength;
	fileHeader->keyLength = attrLength + sizeof(RID);
	fileHeader->attrType = attrType;
	fileHeader->rootPage = 1;
	fileHeader->first_leaf = 1;
	fileHeader->order = (PF_PAGE_SIZE - sizeof(IX_FileHeader) - sizeof(IX_Node) - PTR_SIZE) / (PTR_SIZE + attrLength);

	IX_Node* IXNode = getIXNodefromPH(&pageHandle);
	IXNode->is_leaf = 1;
	IXNode->keynum = 0;
	IXNode->parent = 0;
	IXNode->brother = 0;
	IXNode->keys = pageHandle.pFrame->page.pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	IXNode->rids = (RID*)(IXNode->keys + fileHeader->order * attrLength);

	//�޸�ҳ������PF�к������д���
	MarkDirty(&pageHandle);

	UnpinPage(&pageHandle);

	if ((tmp = CloseFile(fileID)) != SUCCESS) {
		return tmp;
	}

	return SUCCESS;
}

RC OpenIndex(const char* fileName, IX_IndexHandle* indexHandle)
{
	if (indexHandle->bOpen == true) {
		return IX_IHOPENNED;
	}

	//���ļ�
	RC tmp;
	int fileID;
	if ((tmp = OpenFile((char*)fileName, &fileID)) != SUCCESS) {
		return tmp;
	}

	//��ȡ����������Ϣ
	PF_PageHandle pageHandle;
	if ((tmp = GetThisPage(fileID, 1, &pageHandle)) != SUCCESS) {
		return tmp;
	}

	indexHandle->bOpen = true;
	indexHandle->fileID = fileID;
	indexHandle->fileHeader = *(IX_FileHeader*)pageHandle.pFrame->page.pData;

	UnpinPage(&pageHandle);

	return SUCCESS;
}

RC CloseIndex(IX_IndexHandle* indexHandle)
{
	if (indexHandle->bOpen == false) {
		return IX_IHCLOSED;
	}

	indexHandle->bOpen = false;
	RC tmp;
	if ((tmp = CloseFile(indexHandle->fileID)) != SUCCESS) {
		return tmp;
	}

	return SUCCESS;
}

RC InsertEntry(IX_IndexHandle* indexHandle, void* pData, const RID* rid)
{
	if (indexHandle->bOpen == false) {
		return IX_IHCLOSED;
	}

	RC tmp;
	IndexInfo indexinfo(indexHandle);
	int order = indexHandle->fileHeader.order;
	PF_PageHandle pageHandle;

	tmp = FindFirstIXNode(indexHandle, (char*)pData, &pageHandle);
	if (tmp != SUCCESS) {
		return tmp;
	}

	int pn = pageHandle.pFrame->page.pageNum;

	UnpinPage(&pageHandle);

	InsertEntryInLeaf(indexHandle, pData, rid, pn);
	
	return SUCCESS;
}

RC DeleteEntry(IX_IndexHandle* indexHandle, void* pData, const RID* rid)
{
	if (indexHandle->bOpen == false) {
		return IX_IHCLOSED;
	}

	RC tmp;
	PF_PageHandle pageHandle;

	tmp = FindIXNodeInRid(indexHandle, (char*)pData, rid, &pageHandle);
	if (tmp != SUCCESS) {
		return IX_NOMEM;
	}
	
	int pn = pageHandle.pFrame->page.pageNum;

	UnpinPage(&pageHandle);

	DeleteNode(indexHandle, pData, rid, pn);

	return SUCCESS;
}

RC OpenIndexScan(IX_IndexScan* indexScan, IX_IndexHandle* indexHandle, CompOp compOp, char* value)
{
	if (indexScan->bOpen == true) {
		return IX_SCANOPENNED;
	}

	RC tmp;
	PF_PageHandle pageHandle;
	PageNum pn;
	
	tmp = FindFirstIXNode(indexHandle, value, &pageHandle);
	if (tmp != SUCCESS) return tmp;

	IX_Node* ixNode = getIXNodefromPH(&pageHandle);
	//����ֱ�ӷ���
	if (ixNode->keynum == 0) {
		return IX_NOMEM;
	}

	pn = ixNode->brother;
	IndexInfo indexinfo(indexHandle);
	char* keyval = (char*)malloc(indexinfo.attrLength);

	int ridIx = 0, first = 0;
	switch (compOp)
	{
	case EQual:
		while (ridIx < ixNode->keynum) {
			memmove(keyval, ixNode->keys + ridIx * indexinfo.attrLength, indexinfo.attrLength);
			int result = CompareValue(indexinfo.attrType, keyval, value);
			if (result < 0) {
				ridIx++;
			}
			else if (result == 0){
				break;
			}
			else {
				return IX_NOMEM;
			}
		}

		if (ridIx == ixNode->keynum) {
			return IX_NOMEM;
		}
		break;
	case LEqual:
		first = 1;
		break;
	case NEqual:
		return FAIL;
		break;
	case LessT:
		first = 1;
		break;
	case GEqual:
		while (ridIx < ixNode->keynum) {
			memmove(keyval, ixNode->keys + ridIx * indexinfo.attrLength, indexinfo.attrLength);
			int result = CompareValue(indexinfo.attrType, keyval, value);
			if (result < 0) {
				ridIx++;
			}
			else {
				break;
			}
		}

		if (ridIx == ixNode->keynum) {
			return IX_NOMEM;
		}
		break;
	case GreatT:
		while (1) {
			//�����ȡ�����һ��Ҷ�ӽڵ�����һ��������ͷ���EOF
			if (ridIx == ixNode->keynum && ixNode->brother == 0) {
				return IX_EOF;
			}

			//��������ýڵ����һ����������л�����һҳ��
			if (ridIx == ixNode->keynum) {
				UnpinPage(&pageHandle);
				if ((tmp = GetThisPage(indexHandle->fileID, ixNode->brother, &pageHandle)) != SUCCESS) {
					return tmp;
				}
				pn = ixNode->brother;
				ixNode = getIXNodefromPH(&pageHandle);
				ridIx = 0;
			}
			memmove(keyval, ixNode->keys + ridIx * indexinfo.attrLength, indexinfo.attrLength);
			int result = CompareValue(indexinfo.attrType, keyval, value);
			if (result <= 0) {
				ridIx++;
			}
			else {
				break;
			}
		}
		break;
	case NO_OP:
		first = 1;
		break;
	default:
		break;
	}

	if (first == 1) {
		UnpinPage(&pageHandle);
		if ((tmp = GetThisPage(indexHandle->fileID, indexHandle->fileHeader.first_leaf, &pageHandle)) != SUCCESS) {
			return tmp;
		}
		IX_Node* ixNode = getIXNodefromPH(&pageHandle);
		pn = ixNode->brother;
	}

	indexScan->bOpen = true;
	indexScan->pIXIndexHandle = indexHandle;
	indexScan->compOp = compOp;
	indexScan->value = value;
	indexScan->PageHandle = pageHandle;
	indexScan->pn = pn;
	indexScan->ridIx = ridIx;

	return SUCCESS;
}

RC IX_GetNextEntry(IX_IndexScan* indexScan, RID* rid)
{
	if (indexScan->bOpen == false) {
		return IX_ISCLOSED;
	}

	//��PH�ڴ��л�ȡIX_Node��Ϣ
	IX_Node* ixNode = getIXNodefromPH(&indexScan->PageHandle);
	RC tmp;

	//�����ȡ�����һ��Ҷ�ӽڵ�����һ��������ͷ���EOF
	if (indexScan->ridIx == ixNode->keynum && indexScan->pn == 0) {
		return IX_EOF;
	}

	//��������ýڵ����һ����������л�����һҳ��
	if (indexScan->ridIx == ixNode->keynum) {
		UnpinPage(&indexScan->PageHandle);
		if ((tmp = GetThisPage(indexScan->pIXIndexHandle->fileID, indexScan->pn, &indexScan->PageHandle)) != SUCCESS) {
			return tmp;
		}
		ixNode = getIXNodefromPH(&indexScan->PageHandle);
		indexScan->pn = ixNode->brother;
		indexScan->ridIx = 0;
	}

	//��ȡ����ֵ�������Ϣ
	IndexInfo indexinfo(indexScan->pIXIndexHandle);
	char*  keyval = (char*)malloc(indexinfo.attrLength);
	memmove(keyval, ixNode->keys + indexScan->ridIx * indexinfo.attrLength, indexinfo.attrLength);
	int result = CompareValue(indexinfo.attrType, keyval, indexScan->value);
	free(keyval);
	//���ݲ������Խ�����д���
	switch (indexScan->compOp)
	{
	case EQual:
		if (result != 0) {
			return IX_NOMOREIDXINMEM;
		}
		break;
	case LEqual:
		if (result > 0) {
			return IX_NOMOREIDXINMEM;
		}
		break;
	case NEqual:
		return FAIL;
		break;
	case LessT:
		if (result >= 0) {
			return IX_NOMOREIDXINMEM;
		}
		break;
	case GEqual:
		break;
	case GreatT:
		break;
	case NO_OP:
		break;
	default:
		break;
	}

	*rid = *(ixNode->rids + indexScan->ridIx);
	indexScan->ridIx++;

	return SUCCESS;
}

RC CloseIndexScan(IX_IndexScan* indexScan)
{
	if (indexScan->bOpen == false) {
		return IX_SCANCLOSED;
	}

	indexScan->bOpen = false;

	UnpinPage(&indexScan->PageHandle);

	return SUCCESS;
}



int CompareValue(AttrType attrType, char* keyval, char* value) {
	int result;
	float fres = *(float*)keyval - *(float*)value;
	//�����������ͻ�ȡ�ȽϽ��
	switch (attrType)
	{
	case ints:
		result = *(int*)keyval - *(int*)value;
		break;
	case chars:
		result = strcmp(keyval,value);
		break;
	case floats:
		if (fres < 0) {
			result = -1;
		}
		else if (fres > 0) {
			result = 1;
		}
		else {
			result = 0;
		}
		break;
	default:
		result = strcmp(keyval, value);
		break;
	}

	return result;
}


RC FindFirstIXNode(IX_IndexHandle* indexHandle, char* value, PF_PageHandle* pageHandle) {
	//�Ӹ��ڵ㿪ʼ������Ҫ��ֵ
	PageNum _pn = indexHandle->fileHeader.rootPage;

	IX_Node* ixNode;
	IndexInfo indexinfo(indexHandle);
	char* _keyval = (char*)malloc(indexinfo.attrLength);

	PF_PageHandle _pageHandle;
	RC tmp;
	int _ridIx;
	while(1){

		//ÿ�ζ�ȡҳ��Ϊ_pn��ҳ����д���
		if ((tmp = GetThisPage(indexHandle->fileID, _pn, &_pageHandle)) != SUCCESS) {
			free(_keyval);
			return tmp;
		}

		//��ȡ�ڵ���Ϣ�������Ҷ�ӽڵ��ֱ�ӷ���
		ixNode = getIXNodefromPH(&_pageHandle);
		if (ixNode->is_leaf == 1) {
			break;
		}

		//��ȡ�ڵ������ֵ������ĵ�ֵ���бȽϣ�����ֵС�ڸ���ֵ������ƶ�
		int result = -1;
		for (_ridIx = 0; _ridIx < ixNode->keynum; _ridIx++) {
			memmove(_keyval, ixNode->keys + _ridIx * indexinfo.attrLength, indexinfo.attrLength);
			result = CompareValue(indexinfo.attrType, _keyval, value);
			if (result > 0) {
				break;
			}
		}
		_pn = *(PageNum*)(ixNode->rids + _ridIx);
		UnpinPage(&_pageHandle);
	} 
	*pageHandle = _pageHandle;
	free(_keyval);
	return SUCCESS;
}

RC FindIXNodeInRid(IX_IndexHandle* indexHandle, char* keyval, const RID* ridval, PF_PageHandle* pageHandle) {
	//���ȵ��ò���RID�����жϵ�FindIXNode��keyval���з�Χ�ų�
	RC tmp;
	PF_PageHandle _pageHandle;
	PageNum _pn;
	if ((tmp = FindFirstIXNode(indexHandle, keyval, &_pageHandle)) != SUCCESS) {
		return tmp;
	}
	
	//ͨ��indexHandle��ȡ�����������Ϣ
	IX_Node* ixNode;
	IndexInfo indexinfo(indexHandle);
	char* _keyval = (char*)malloc(indexinfo.attrLength);
	RID _ridval;
	
	int _ridIx = 0;
	int success = 0, fail = 0;//��������ѭ��
	while (!success && !fail) {
		//��ȡ�ڵ���Ϣ
		ixNode = getIXNodefromPH(&_pageHandle);

		//��ȡ�ڵ������ֵ������ĵ�ֵ���бȽϣ�����ֵС�ڸ���ֵ������ƶ�
		for (_ridIx = 0; _ridIx < ixNode->keynum && !success && !fail; _ridIx++) {
			memmove(_keyval, ixNode->keys + _ridIx * indexinfo.attrLength, indexinfo.attrLength);
			int result1 = CompareValue(indexinfo.attrType, _keyval, keyval);
			if (result1 == 0) {
				memmove(&_ridval, ixNode->rids + _ridIx, PTR_SIZE);
				if (memcmp(&_ridval, ridval, PTR_SIZE) == 0) {
					*pageHandle = _pageHandle;
					success = 1;
				}
			}
			else if (result1 < 0) {
				continue;
			}
			else {
				fail = 1;
			}
		}

		if (success || fail) {
			break;
		}

		//�������Ҳû�ҵ����л���һ��Ҷ�ӽڵ�
		_pn = ixNode->brother;
		_ridIx = 0;
		UnpinPage(&_pageHandle);

		//�����ȡ�����һ��Ҷ�ӽڵ��ֱ�ӷ���
		if (_pn == 0) {
			fail = 1;
			break;
		}

		if ((tmp = GetThisPage(indexHandle->fileID, _pn, &_pageHandle)) != SUCCESS) {
			free(_keyval);
			return tmp;
		}
	}
	
	free(_keyval);
	if (fail) {
		return FAIL;
	}
	*pageHandle = _pageHandle;
	UnpinPage(&_pageHandle);

	return SUCCESS;
}


RC InsertEntryInLeaf(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn) {

	RC tmp;
	IndexInfo indexinfo(indexHandle);
	int order = indexHandle->fileHeader.order;
	PF_PageHandle pageHandle;

	if ((tmp = GetThisPage(indexHandle->fileID, pn, &pageHandle)) != SUCCESS) {
		return tmp;
	}

	IX_Node* ixNode = getIXNodefromPH(&pageHandle);

	//Ѱ�Ҳ����½ڵ��λ��
	char* keyval = (char*)malloc(indexinfo.attrLength);
	int i;
	for (i = 0; i < ixNode->keynum; i++) {
		memmove(keyval, ixNode->keys + i * indexinfo.attrLength, indexinfo.attrLength);
		int result = CompareValue(indexinfo.attrType, keyval, (char*)pData);
		if (result >= 0) {
			break;
		}
	}
	int _index = i;
	free(keyval);

	//����ﵽ�˷�֧���ޣ��ͽ��з���
	if (ixNode->keynum == order) {
		PF_PageHandle newpageHandle;
		if ((tmp = AllocatePage(indexHandle->fileID, &newpageHandle)) != SUCCESS) {
			return tmp;
		}

		//�����½ڵ㣬�޸Ķ�Ӧ�ĳ�Ա��ֵ
		IX_Node* newixNode = getIXNodefromPH(&newpageHandle);
		newixNode->is_leaf = ixNode->is_leaf;
		newixNode->brother = ixNode->brother;
		ixNode->brother = newpageHandle.pFrame->page.pageNum;
		newixNode->parent = ixNode->parent;
		newixNode->keys = newpageHandle.pFrame->page.pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
		newixNode->rids = (RID*)(newixNode->keys + order * indexinfo.attrLength);

		//��ǰһ�ڵ�ĺ������ת�Ƶ��½ڵ��У�ԭ�пռ����
		memmove(newixNode->keys, ixNode->keys + order / 2 * indexinfo.attrLength, (order + 1) / 2 * indexinfo.attrLength);
		memset(ixNode->keys + order / 2 * indexinfo.attrLength, 0, (order + 1) / 2 * indexinfo.attrLength);
		memmove(newixNode->rids, ixNode->rids + order / 2, (order + 1) / 2 * PTR_SIZE);
		memset(ixNode->rids + order / 2, 0, (order + 1) / 2 * PTR_SIZE);
		newixNode->keynum = (order + 1) / 2;
		ixNode->keynum -= newixNode->keynum;

		//���ݲ���ڵ�λ����ѡ��������½ڵ㻹�Ǿɽڵ���
		if (_index <= order / 2) {
			memmove(ixNode->keys + (_index + 1) * indexinfo.attrLength, ixNode->keys + _index * indexinfo.attrLength, (order / 2 - _index) * indexinfo.attrLength);
			memmove(ixNode->keys + _index * indexinfo.attrLength, pData, indexinfo.attrLength);
			memmove(ixNode->rids + _index + 1, ixNode->rids + _index, order / 2 * PTR_SIZE);
			memmove(ixNode->rids + _index, rid, PTR_SIZE);
			ixNode->keynum++;
		}
		else {
			_index -= order / 2;
			memmove(newixNode->keys + (_index + 1) * indexinfo.attrLength, newixNode->keys + _index * indexinfo.attrLength, ((order + 1) / 2 - _index) * indexinfo.attrLength);
			memmove(newixNode->keys + _index * indexinfo.attrLength, pData, indexinfo.attrLength);
			memmove(newixNode->rids + _index + 1, newixNode->rids + _index, ((order + 1) / 2 - _index) * PTR_SIZE);
			memmove(newixNode->rids + _index, rid, PTR_SIZE);
			newixNode->keynum++;
		}

		int pageNum = newpageHandle.pFrame->page.pageNum;
		void* data = malloc(indexinfo.attrLength);
		memmove(data, newixNode->keys, indexinfo.attrLength);

		MarkDirty(&newpageHandle);
		UnpinPage(&newpageHandle);

		//�����ǰ���ѵĲ��Ǹ��ڵ㣬��ô��Ҫ�򸸽ڵ����һ���ڵ㣻�����ǰ���ѵ��Ǹ��ڵ㣬��ô��Ҫ�����µĸ��ڵ�
		if (pageHandle.pFrame->page.pageNum != indexHandle->fileHeader.rootPage) {
			 InsertEntryInParent(indexHandle, data, (RID*)&pageNum, ixNode->parent);
		}
		else {
			PF_PageHandle rootpageHandle;
			if ((tmp = AllocatePage(indexHandle->fileID, &rootpageHandle)) != SUCCESS) {
				return tmp;
			}
			IX_Node* rootixNode = getIXNodefromPH(&rootpageHandle);
			rootixNode->keys = rootpageHandle.pFrame->page.pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
			rootixNode->rids = (RID*)(rootixNode->keys + order * indexinfo.attrLength);
			rootixNode->brother = 0;
			rootixNode->is_leaf = 0;
			rootixNode->parent = 0;
			indexHandle->fileHeader.rootPage = rootpageHandle.pFrame->page.pageNum;
			ixNode->parent = rootpageHandle.pFrame->page.pageNum;
			newixNode->parent = rootpageHandle.pFrame->page.pageNum;
			memmove(rootixNode->keys, data, indexinfo.attrLength);
			memmove(rootixNode->rids, (RID*)&pn, PTR_SIZE);
			memmove(rootixNode->rids + 1, (RID*)&pageNum, PTR_SIZE);
			rootixNode->keynum++;
			MarkDirty(&rootpageHandle);
			UnpinPage(&rootpageHandle);
		}

		free(data);
	}
	else { //δ�ﵽ��֧���ޣ�ֱ�Ӳ���
		memmove(ixNode->keys + (_index + 1) * indexinfo.attrLength, ixNode->keys + _index * indexinfo.attrLength, (ixNode->keynum - _index) * indexinfo.attrLength);
		memmove(ixNode->keys + _index * indexinfo.attrLength, pData, indexinfo.attrLength);
		memmove(ixNode->rids + _index + 1, ixNode->rids + _index, (ixNode->keynum - _index) * PTR_SIZE);
		memmove(ixNode->rids + _index, rid, PTR_SIZE);
		ixNode->keynum++;
	}

	MarkDirty(&pageHandle);
	UnpinPage(&pageHandle);

	return SUCCESS;
}

RC InsertEntryInParent(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn) {

	RC tmp;
	IndexInfo indexinfo(indexHandle);
	int order = indexHandle->fileHeader.order;
	PF_PageHandle pageHandle;

	if ((tmp = GetThisPage(indexHandle->fileID, pn, &pageHandle)) != SUCCESS) {
		return tmp;
	}

	IX_Node* ixNode = getIXNodefromPH(&pageHandle);

	//Ѱ�Ҳ����½ڵ��λ��
	char* keyval = (char*)malloc(indexinfo.attrLength);
	int i;
	for (i = 0; i < ixNode->keynum; i++) {
		memmove(keyval, ixNode->keys + i * indexinfo.attrLength, indexinfo.attrLength);
		int result = CompareValue(indexinfo.attrType, keyval, (char*)pData);
		if (result >= 0) {
			break;
		}
	}
	int _index = i;
	free(keyval);

	//����ﵽ�˷�֧���ޣ��ͽ��з���
	if (ixNode->keynum == order) {
		PF_PageHandle newpageHandle;
		if ((tmp = AllocatePage(indexHandle->fileID, &newpageHandle)) != SUCCESS) {
			return tmp;
		}

		//�����½ڵ㣬�޸Ķ�Ӧ�ĳ�Ա��ֵ
		IX_Node* newixNode = getIXNodefromPH(&newpageHandle);
		newixNode->is_leaf = ixNode->is_leaf;
		newixNode->brother = ixNode->brother;
		ixNode->brother = newpageHandle.pFrame->page.pageNum;
		newixNode->parent = ixNode->parent;
		newixNode->keys = newpageHandle.pFrame->page.pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
		newixNode->rids = (RID*)(newixNode->keys + order * indexinfo.attrLength);

		int pageNum = newpageHandle.pFrame->page.pageNum;
		void* data = malloc(indexinfo.attrLength);

		//���ݲ���ڵ�λ����ѡ��������½ڵ㻹�Ǿɽڵ��У�����ѡ����뵽�ϲ������
		if (_index == order / 2) {
			//���һ��������ڵ�ֵ��Ϊ����ֵ���ݸ����ڵ�
			memmove(data, pData, indexinfo.attrLength);

			//��ԭ�ڵ��������������½ڵ�
			memmove(newixNode->keys, ixNode->keys + order / 2 * indexinfo.attrLength, (order + 1) / 2 * indexinfo.attrLength);
			memset(ixNode->keys + order / 2 * indexinfo.attrLength, 0, (order + 1) / 2 * indexinfo.attrLength);
			//�ƶ�ָ����
			memmove(newixNode->rids + 1, ixNode->rids + order / 2, (order + 1) / 2 * PTR_SIZE);
			memset(ixNode->rids + order / 2, 0, (order + 1) / 2 * PTR_SIZE);
			memmove(newixNode->rids, rid, PTR_SIZE);
		}
		else if (_index < order / 2) {
			//���������order/2 - 1λ�õĽڵ���Ϊ����ֵ���ݸ����ڵ�
			memmove(data, ixNode->keys + (order / 2 - 1) * indexinfo.attrLength, indexinfo.attrLength);

			//�ƶ�������½ڵ�
			memmove(newixNode->keys, ixNode->keys + order / 2 * indexinfo.attrLength, (order + 1) / 2 * indexinfo.attrLength);
			memset(ixNode->keys + order / 2 * indexinfo.attrLength, 0, (order + 1) / 2 * indexinfo.attrLength);
			//������������
			memmove(ixNode->keys + (_index + 1) * indexinfo.attrLength, ixNode->keys + _index * indexinfo.attrLength, (order / 2 - _index) * indexinfo.attrLength);
			memmove(ixNode->keys + _index * indexinfo.attrLength, pData, indexinfo.attrLength);
			//�ƶ�ָ����½ڵ�
			memmove(newixNode->rids, ixNode->rids + order / 2, (order + 3) / 2 * PTR_SIZE);
			memset(ixNode->rids + order / 2, 0, (order + 3) / 2 * PTR_SIZE);
			//������ָ����
			memmove(ixNode->rids + _index + 2, ixNode->rids + _index + 1, ((order + 3) / 2 - _index) * PTR_SIZE);
			memmove(ixNode->rids + _index + 1, rid, PTR_SIZE);
		}
		else {
			//���������order/2λ�õĽڵ���Ϊ����ֵ���ݸ����ڵ�
			_index -= order / 2 + 1;
			memmove(data, ixNode->keys + order / 2 * indexinfo.attrLength, indexinfo.attrLength);

			//�ƶ�������½ڵ�
			memmove(newixNode->keys, ixNode->keys + (order / 2 + 1) * indexinfo.attrLength, (order - 1) / 2 * indexinfo.attrLength);
			memset(ixNode->keys + (order / 2 + 1) * indexinfo.attrLength, 0, (order - 1) / 2 * indexinfo.attrLength);
			//������������
			memmove(newixNode->keys + (_index + 1) * indexinfo.attrLength, newixNode->keys + _index * indexinfo.attrLength, ((order - 1) / 2 - _index) * indexinfo.attrLength);
			memmove(newixNode->keys + _index * indexinfo.attrLength, pData, indexinfo.attrLength);
			//�ƶ�ָ����½ڵ�
			memmove(newixNode->rids, ixNode->rids + order / 2 + 1, (order + 1) / 2 * PTR_SIZE);
			memset(ixNode->rids + order / 2 + 1, 0, (order + 1) / 2 * PTR_SIZE);
			//������ָ����
			memmove(newixNode->rids + _index + 2, newixNode->rids + _index + 1, ((order - 1) / 2 - _index) * PTR_SIZE);
			memmove(newixNode->rids + _index + 1, rid, PTR_SIZE);
		}

		newixNode->keynum = (order + 1) / 2;
		ixNode->keynum -= newixNode->keynum;

		MarkDirty(&newpageHandle);
		UnpinPage(&newpageHandle);

		//�����ǰ���ѵĲ��Ǹ��ڵ㣬��ô��Ҫ�򸸽ڵ����һ���ڵ㣻�����ǰ���ѵ��Ǹ��ڵ㣬��ô��Ҫ�����µĸ��ڵ�
		if (pageHandle.pFrame->page.pageNum != indexHandle->fileHeader.rootPage) {
			InsertEntryInParent(indexHandle, data, (RID*)&pageNum, ixNode->parent);
		}
		else {
			PF_PageHandle rootpageHandle;
			if ((tmp = AllocatePage(indexHandle->fileID, &rootpageHandle)) != SUCCESS) {
				return tmp;
			}
			IX_Node* rootixNode = getIXNodefromPH(&rootpageHandle);
			rootixNode->keys = rootpageHandle.pFrame->page.pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
			rootixNode->rids = (RID*)(rootixNode->keys + order * indexinfo.attrLength);
			rootixNode->brother = 0;
			rootixNode->is_leaf = 0;
			rootixNode->parent = 0;
			indexHandle->fileHeader.rootPage = rootpageHandle.pFrame->page.pageNum;
			ixNode->parent = rootpageHandle.pFrame->page.pageNum;
			newixNode->parent = rootpageHandle.pFrame->page.pageNum;
			memmove(rootixNode->keys, data, indexinfo.attrLength);
			memmove(rootixNode->rids, (RID*)&pn, PTR_SIZE);
			memmove(rootixNode->rids + 1, (RID*)&pageNum, PTR_SIZE);
			rootixNode->keynum++;
		}

		free(data);
	}
	else { //δ�ﵽ��֧���ޣ�ֱ�Ӳ���
		memmove(ixNode->keys + (_index + 1) * indexinfo.attrLength, ixNode->keys + _index * indexinfo.attrLength, (ixNode->keynum - _index) * indexinfo.attrLength);
		memmove(ixNode->keys + _index * indexinfo.attrLength, pData, indexinfo.attrLength);
		memmove(ixNode->rids + _index + 2, ixNode->rids + _index + 1, (ixNode->keynum - _index) * PTR_SIZE);
		memmove(ixNode->rids + _index + 1, rid, PTR_SIZE);
		ixNode->keynum++;
	}

	MarkDirty(&pageHandle);
	UnpinPage(&pageHandle);

	return SUCCESS;
}


RC DeleteNode(IX_IndexHandle* indexHandle, void* pData, const RID* rid, PageNum pn) {
	RC tmp;
	IndexInfo indexinfo(indexHandle);
	int order = indexHandle->fileHeader.order;
	PF_PageHandle pageHandle;

	tmp = GetThisPage(indexHandle->fileID, pn, &pageHandle);
	if (tmp != SUCCESS) {
		return SUCCESS;
	}

	//Ѱ����Ҫɾ���Ľ��
	IX_Node* ixNode = getIXNodefromPH(&pageHandle);
	char* keyval = (char*)malloc(indexinfo.attrLength);
	int ridIx;
	int i;
	for (i = 0; i < ixNode->keynum; i++) {
		memmove(keyval, ixNode->keys + i * indexinfo.attrLength, indexinfo.attrLength);
		int result = CompareValue(indexinfo.attrType, keyval, (char*)pData);
		if (result == 0) {
			break;
		}
	}
	free(keyval);
	ridIx = i;

	
	//�ӽڵ���ɾ���ҵ��Ľڵ�
	ixNode->keynum--;
	//ɾ��������
	memmove(ixNode->keys + ridIx * indexinfo.attrLength, ixNode->keys + (ridIx + 1) * indexinfo.attrLength, (ixNode->keynum - ridIx) * indexinfo.attrLength);
	memset(ixNode->keys + ixNode->keynum * indexinfo.attrLength, 0, (order - ixNode->keynum) * indexinfo.attrLength);


	//ɾ��ָ�����Ҫ����Ҷ�ӽڵ�ͷ�Ҷ�ӽڵ��������
	if (ixNode->is_leaf) {
		memmove(ixNode->rids + ridIx, ixNode->rids + ridIx + 1, (ixNode->keynum + 1 - ridIx) * PTR_SIZE);
		memset(ixNode->rids + ixNode->keynum, 0, (order - ixNode->keynum) * PTR_SIZE);
	}
	else {
		memmove(ixNode->rids + ridIx + 1, ixNode->rids + ridIx + 2, (ixNode->keynum - ridIx - 1) * PTR_SIZE);
		memset(ixNode->rids + ixNode->keynum + 1, 0, (order - ixNode->keynum) * PTR_SIZE);
	}
	
	//�����ǰ�ڵ��Ǹ��ڵ��ҷ�֧Ϊһ�����ӽڵ��Ϊ�¸��ڵ�
	if (pn == indexHandle->fileHeader.rootPage && ixNode->keynum == 1 && ixNode->is_leaf != 1) {
		indexHandle->fileHeader.rootPage = *(PageNum*)ixNode->rids;
		MarkDirty(&pageHandle);
		UnpinPage(&pageHandle);
		DisposePage(indexHandle->fileID, pn);
		return SUCCESS;
	}

	//�ж�ɾ����ڵ����Ƿ����Ҫ�󣬹��վ���Ҫ����ɾ���ڵ�
	if (indexHandle->fileHeader.rootPage != pn && ixNode->keynum < order / 2) {
		//��ȡ���׽ڵ�
		PF_PageHandle fapageHandle;

		tmp = GetThisPage(indexHandle->fileID, ixNode->parent, &fapageHandle);
		if (tmp != SUCCESS) {
			return SUCCESS;
		}
		IX_Node* faixNode = getIXNodefromPH(&fapageHandle);

		//�ڸ��ڵ��в���Ҫɾ�����ӽڵ��λ�ã���ȡ��ǰ���ֵܵ���Ϣ
		char* ridval = (char*)malloc(PTR_SIZE);
		int i;
		for (i = 0; i <= faixNode->keynum; i++) {
			memmove(ridval, faixNode->rids + i, PTR_SIZE);
			int result = CompareValue(indexinfo.attrType, ridval, (char*)pData);
			if (result >= 0) {
				break;
			}
		}
		free(ridval);
		ridIx = i;
		char* keydata = (char*)malloc(indexinfo.attrLength);
		

		int broPn, poz = -1;//broId������ʶ�ֵܽڵ��ҳ��ţ�poz����ָʾ�ֵܽڵ��λ��
		if (ridIx != 0){
			broPn = *(int*)(faixNode->rids + ridIx - 1);
			poz = 0;
			memmove(keydata, faixNode->keys + (ridIx - 1) * indexinfo.attrLength, indexinfo.attrLength);
		}
		else {
			broPn = *(int*)(faixNode->rids + ridIx + 1);
			poz = 1;
			memmove(keydata, faixNode->keys + ridIx * indexinfo.attrLength, indexinfo.attrLength);
		}

		PF_PageHandle bropageHandle;

		tmp = GetThisPage(indexHandle->fileID, broPn, &bropageHandle);
		if (tmp != SUCCESS) {
			return SUCCESS;
		}
		IX_Node* broixNode = getIXNodefromPH(&bropageHandle);
		
		//����ֵܽڵ��������£���ô�ͺϲ������ڵ㣬�������Ҫ������������ƶ�
		if (broixNode->keynum + ixNode->keynum < order) {
			UnpinPage(&fapageHandle);
			//�����ǰ���ֵܣ����л������ڵ������Ϣ��ʹ��ֻ��Ҫ��������ϲ�һ�����
			if (poz == 0) {
				IX_Node* tmp = broixNode;
				broixNode = ixNode;
				ixNode = tmp;
				int tmpp = broPn;
				pn = broPn;
				broPn = tmpp;
				PF_PageHandle tmpph = bropageHandle;
				bropageHandle = pageHandle;
				pageHandle = tmpph;
			}
			
			memmove(ixNode->keys + ixNode->keynum * indexinfo.attrLength, broixNode->keys, broixNode->keynum * indexinfo.attrLength);
			memmove(ixNode->rids + ixNode->keynum, broixNode->rids, broixNode->keynum * PTR_SIZE);
			ixNode->brother = broixNode->brother;
			UnpinPage(&bropageHandle);
			DisposePage(indexHandle->fileID, broPn);
			DeleteNode(indexHandle, keydata, (RID*)&broPn, ixNode->parent);
			MarkDirty(&pageHandle);
			UnpinPage(&pageHandle);
			
		}
		else {//�ֵܽڵ��޷��ϲ��������������ƶ��������

			broixNode->keynum--;

			if (poz == 0) {//�ֵܽڵ�Ϊǰ���ڵ�

				if (ixNode->is_leaf) {//��������ƶ�ǰ���ڵ���������ݼ���

					//�Ƴ�һ����λ���������µ�һ���������ָ����
					memmove(ixNode->keys + indexinfo.attrLength, ixNode->keys , ixNode->keynum * indexinfo.attrLength);
					memmove(ixNode->rids + 1, ixNode->rids, ixNode->keynum * PTR_SIZE);
					//��ǰ���ֵܵ��������ָ�����ƶ�����ǰ�ڵ���
					memmove(ixNode->keys, broixNode->keys + broixNode->keynum * indexinfo.attrLength, indexinfo.attrLength);
					memmove(ixNode->rids, broixNode->rids + broixNode->keynum, PTR_SIZE);
					memset(broixNode->keys + broixNode->keynum * indexinfo.attrLength, 0, indexinfo.attrLength);
					memset(broixNode->rids + broixNode->keynum, 0, PTR_SIZE);
					//�ƶ�����޸ĸ��ڵ��Ӧλ�õ�����������
					memmove(faixNode->keys + (ridIx - 1) * indexinfo.attrLength, ixNode->keys, indexinfo.attrLength);
				}
				else {//��������ƶ�ǰ���ڵ��ָ�룬�����ڵ��������ƶ�����ǰ�ڵ㣬ǰ���ڵ���������ȥ

					//�Ƴ�һ����λ���������µ�һ���������ָ����
					memmove(ixNode->keys + indexinfo.attrLength, ixNode->keys, ixNode->keynum* indexinfo.attrLength);
					memmove(ixNode->rids + 1, ixNode->rids, ixNode->keynum* PTR_SIZE);
					//�����ڵ���������ǰ���ڵ��ָ�����ƶ�����ǰ�ڵ���
					memmove(ixNode->keys, keydata, indexinfo.attrLength);
					memmove(ixNode->rids, broixNode->rids + broixNode->keynum + 1, PTR_SIZE);
					memset(broixNode->rids + broixNode->keynum + 1, 0, PTR_SIZE);
					//�ƶ�����޸ĸ��ڵ��Ӧλ�õ����������ݲ����ǰ���ڵ��Ӧ������λ�õ�����
					memmove(faixNode->keys + (ridIx - 1) * indexinfo.attrLength, broixNode->keys + broixNode->keynum * indexinfo.attrLength, indexinfo.attrLength);
					memset(broixNode->keys + broixNode->keynum * indexinfo.attrLength, 0, indexinfo.attrLength);
				}
			}
			else {//�ֵܽڵ�Ϊ��̽ڵ�����

				if (ixNode->is_leaf) {//�ƶ���̽ڵ�ĵ�һ�����������ݼ���

					//���ƶ���̽ڵ�ĵ�һ���������ָ�����ǰ�ڵ���
					memmove(ixNode->keys, broixNode->keys, indexinfo.attrLength);
					memmove(ixNode->rids, broixNode->rids, PTR_SIZE);
					//����̽ڵ����������ǰ�����ȱ
					memmove(broixNode->keys, broixNode->keys + indexinfo.attrLength, broixNode->keynum * indexinfo.attrLength);
					memmove(broixNode->rids, broixNode->rids + 1, (broixNode->keynum + 1) * PTR_SIZE);
					//�޸ĸ��ڵ��ж�Ӧ����
					memmove(faixNode->keys + ridIx * indexinfo.attrLength, broixNode->keys, indexinfo.attrLength);
				}
				else {//�ƶ���̽ڵ��һ��ָ����͸��ڵ��е��������ǰ�ڵ㼴��
					
					//���ƶ�ָ�����������
					memmove(ixNode->rids, broixNode->rids, PTR_SIZE);
					memmove(ixNode->keys, faixNode->keys + ridIx * indexinfo.attrLength, indexinfo.attrLength);
					//�����ڵ��Ӧ���ݸ�Ϊ��̽ڵ�ĵ�һ��������
					memmove(faixNode->keys + ridIx * indexinfo.attrLength, broixNode->keys, indexinfo.attrLength);
					//����̽ڵ����������ǰ�����ȱ
					memmove(broixNode->keys, broixNode->keys + indexinfo.attrLength, broixNode->keynum * indexinfo.attrLength);
					memmove(broixNode->rids, broixNode->rids + 1, (broixNode->keynum + 1) * PTR_SIZE);
				}
			}
			ixNode->keynum++;
		}
		MarkDirty(&fapageHandle);
		UnpinPage(&fapageHandle);
		MarkDirty(&bropageHandle);
		UnpinPage(&bropageHandle);
		MarkDirty(&pageHandle);
		UnpinPage(&pageHandle);
		free(keydata);
	}
	return SUCCESS;
}

Tree_Node* getTree(IX_IndexHandle* indexHandle, IX_Node* pIX_Node, Tree_Node* parent, int childs)//ÿ���ڵ�ֻ�ñ�������ߵĺ��ӽڵ�����ֵܽڵ�
{
	//����һ���ڵ㣬���ڸ��ڵ���ʣ����ӽڵ����-1
	childs--;
	PF_PageHandle pageHandle;
	Tree_Node* pNode = (Tree_Node*)malloc(sizeof(Tree_Node));
	if (pNode == NULL)
		return nullptr;
	//����Ҫ���ĸ�ֵ����
	pNode->keyNum = pIX_Node->keynum; //����Ϊ0--����Ϊ��
	if (pNode->keyNum != 0)
	{
		pNode->keys = (char**)malloc(pNode->keyNum * sizeof(char*));
		if (pNode->keys == NULL)
			return nullptr;
	}
	else
		pNode->keys = nullptr;
	for (int i = 0; i < pNode->keyNum; i++)
	{
		pNode->keys[i] = (char*)malloc(indexHandle->fileHeader.attrLength);
		if (pNode->keys[i] == NULL)
			return nullptr;
		memcpy(pNode->keys[i], pIX_Node->keys + i * indexHandle->fileHeader.keyLength, indexHandle->fileHeader.attrLength);
	}
	pNode->parent = parent;
	//��ǰ�ڵ�Ϊ��Ҷ�ӽڵ�-�м�ڵ�
	if (pIX_Node->is_leaf == 0)
	{
		PageNum first = pIX_Node->rids->pageNum;
		int keyNum = pIX_Node->keynum;
		GetThisPage(indexHandle->fileID, first, &pageHandle);
		IX_Node* pIX = (IX_Node*)(pageHandle.pFrame->page.pData + sizeof(IX_FileHeader));
		pIX->keys = (char*)(pIX + 1);
		pIX->rids = (RID*)(pIX->keys + indexHandle->fileHeader.order * indexHandle->fileHeader.keyLength);
		pNode->firstChild = getTree(indexHandle, pIX, pNode, keyNum);
		UnpinPage(&pageHandle);
	}
	else//��ǰ�ڵ�ΪҶ�ӽڵ�
		pNode->firstChild = nullptr;
	if (childs == 0)
		pNode->sibling = nullptr;
	else
	{
		GetThisPage(indexHandle->fileID, pIX_Node->brother, &pageHandle);
		IX_Node* pIX = (IX_Node*)(pageHandle.pFrame->page.pData + sizeof(IX_FileHeader));
		pIX->keys = (char*)(pIX + 1);
		pIX->rids = (RID*)(pIX->keys + indexHandle->fileHeader.order * indexHandle->fileHeader.keyLength);
		pNode->sibling = getTree(indexHandle, pIX, parent, childs);
		UnpinPage(&pageHandle);
	}
	return pNode;
}

RC GetIndexTree(char* fileName, Tree* index) {
	IX_IndexHandle indexHandle;
	if (OpenIndex(fileName, &indexHandle) != SUCCESS)
		return INDEX_NOT_EXIST;
	index->attrType = indexHandle.fileHeader.attrType;
	index->attrLength = indexHandle.fileHeader.attrLength;
	index->order = indexHandle.fileHeader.order;
	PF_PageHandle pageHandle;
	GetThisPage(indexHandle.fileID, indexHandle.fileHeader.rootPage, &pageHandle);
	IX_Node* pIX_Node = (IX_Node*)(pageHandle.pFrame->page.pData + sizeof(IX_FileHeader));
	pIX_Node->keys = (char*)(pIX_Node + 1);
	pIX_Node->rids = (RID*)(pIX_Node->keys + indexHandle.fileHeader.order * indexHandle.fileHeader.keyLength);
	index->root = getTree(&indexHandle, pIX_Node, nullptr, 1);
	UnpinPage(&pageHandle);
	CloseIndex(&indexHandle);
	return SUCCESS;
};