// Copyright (C) 1991 - 1999 Rational Software Corporation

#include "../headtldb.h"
#include "cdataset.h"
#include "cpage.h"
#include "cpages.h"
#include "cfakerecnos.h"
#include "cfield.h"
#include "cfields.h"

//#include "TlDatabase.h"//###_Mod 2002-10-7 zlq

/************************************************************************************
函数名称：
    CPage::CPage 
功能说明：
    构造函数
详细解释：
出入参数：
	[in]pOwner:本页面所属的页面集指针.
返回类型：
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
CPage::CPage(CPages* pOwner)
{
	m_pPages = pOwner;	   //设定所属的页面集
	m_bIsModify = false;   //本页是否改变过

	m_bReachEndError = false;

	m_lngFakeStart=1; //本页起始伪纪录号
	m_lngLStartRecNo = 1;  //本页起始逻辑纪录号
	m_nLogicCount = 0;     //本页逻辑纪录号的个数 不用函数SetLogicCount()
	m_nOriginLoadCount = 0;//本页伪纪录个数

	m_nCurNo = -1;			//当前逻辑纪录号 
	m_pFields =0;		   //指向CFields 的指针



	m_pPages->m_PageArraySort.AddPage(this);
	m_DatasArray.SetSize(0,_IncreaseNum);
}

/************************************************************************************
函数名称：
    CPage::~CPage
功能说明：
    析构函数
详细解释：
    
出入参数：
返回类型：
    
制作：
    Eric2002-07-24
修改: 
************************************************************************************/
CPage::~CPage()
{
	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		//#_修改 2002-11-26 $ 9:09:41 zlq
		pField->pg_Delete(m_DatasArray[i]);
	}
	m_pPages->m_PageArraySort.RemovePage(this);
}


/************************************************************************************
函数名称：
    CPage::Load
功能说明：
    把"装入申请"放入IO队列中
详细解释：
	1。先调用页面集链表的GetLogicToFake得到偏移，计算出为偏移
	2。修改本页面的成员变量m_lngLStartRecNo，m_lngFakeStartRecNo
	3。负责修改自己的页面状态为等待装入状态
	4。调用伪记录链表的进栈操作
出入参数：
    [in]logicRecNo：装入的起始逻辑纪录号
    [in]count：	装入的个数.
    [in]IsNeedFeedback：是否要等着用.
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
void CPage::Load(int logicRecNo,int count, bool IsNeedFeedback)
{
	//计算出起始伪纪录号,装入页面时使用.
	int iOffset;
	iOffset = m_pPages->GetLogicToFake(logicRecNo);
	//m_pPages->PrintPages();
	//assert( logicRecNo - iOffset>0);
	//设置此页起始逻辑纪录号
	m_lngLStartRecNo = logicRecNo;	
	m_lngFakeStart = logicRecNo - iOffset ;
	m_nOriginLoadCount = count;
	SetLogicCount(count);//730 add

	//申请装入,放到装入队列,并设置页状态
	//SetPageState(pgWaitLoad);
	//修改相关变量
	m_bIsModify = false;
	
	m_pPages->m_pFakeRecNos->InQueueLoad(this,m_lngFakeStart,count,IsNeedFeedback);
}

/************************************************************************************
函数名称：
    CPage::UnLoad
功能说明：
	写入数据,立即返回差距个数（本页面的添加删除操作的相减)
	负责修改自己的页面状态为等待状态写回
详细解释：
	（由于是单通道，所以可以由上级根据返回值调整其它比自己大的页面的旧起始位置）
出入参数：
	[in]bIsNeedFeedBack:是否等着用.
返回类型：
    int	:返回物理纪录与逻辑纪录的差值.
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::UnLoad(bool bIsNeedFeedBack)
{
	//立即返回差距个数
	int nOffset;
	nOffset = m_nLogicCount - m_nOriginLoadCount;

	
	//放到UnLoad队列中,并修改自己的页面状态,其他状态先不修改
	m_pPages->m_pFakeRecNos->UnLoad(this);
	m_nOriginLoadCount = m_nLogicCount;
	//SetPageState(pgUnLoading);
	m_bIsModify = false;
	
	return nOffset;
}

/************************************************************************************
函数名称：
    CPage::HangFields
功能说明：
    将字段集和数据集联系在一起
详细解释：
    实际上调整各字段的m_pValue指向
    此函数最好由上级调用
出入参数：
    [in,out]pFields：  字段集的指针
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
void CPage::HangFields(CFields* pFields)
{
	m_pFields=pFields;
	if(pFields)
		pFields->m_pDataSet =  this;
}

/************************************************************************************
函数名称：
    CPage::BindFields
功能说明：
    将数据集中的数据地址赋到各字段的数据指针中
详细解释：
出入参数：
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
void CPage::BindFields()
{  
	//警告：不能用逻辑空页来判断，而是用物理空页，但不能用IsPhysicalEmpty（）

	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		pField->m_pValue = pField->pg_GetAt(m_DatasArray[i],m_nCurNo);

	}
}







/************************************************************************************
功能说明：
	当页面因不停的添加而变得庞大时需要调整
详细解释：
	将一部分数据拷贝到其它叶面，在此之前，其它叶面应该先写回这个过程由CPages的GetEmptyPage实现。
	同时该过程必须负责将已经传到其它叶面的数据在本叶面中去除
出入参数：
	[in]CopyFlags： 	复制的开始位置
	[in]nPhysicalCount：复制的个数
返回类型：

制作：
	Eric2002-07-03
修改: 
************************************************************************************/
int CPage::CopyToOther(CFromPos CopyFlags, int nPhysicalCount)
{//===uc  CopyToOther
	//取得拷贝的目标页,保存在pPageEmpty中
	CPage * pPageEmpty;
	pPageEmpty = m_pPages->GetPhysicalEmptyPage();
	
	//从页头开始拷贝nPhysicalCount个纪录.
	if( CopyFlags == FromBegin )
	{
		for(int i=0;i<nPhysicalCount;++i)
		{
			for(int j = 0;j<m_DatasArray.GetSize();j++)
			{  //将数据集中的数据地址赋到各字段的数据指针中
				CField * pField = m_pFields->FieldByIndex(j);				
				switch(pField->GetFieldType())
				{
				case fCurrency:
				case fBoolean:
				case fDate:
				case fInt:
				case fDouble:
					((PF_NUM)pPageEmpty->m_DatasArray[j])->Add(((PF_NUM)m_DatasArray[j])->GetAt(i));
					break;
				case fString:
					((PF_STRING)pPageEmpty->m_DatasArray[j])->Add(((PF_STRING)m_DatasArray[j])->GetAt(i));
					break;
				default:
					break;
				}
			}
		}

		//一次性删除
		for(int j = 0;j<m_DatasArray.GetSize();j++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(j);
			switch(pField->GetFieldType())
			{
				case fCurrency:
				case fBoolean:
				case fDate:
				case fInt:
				case fDouble:
				((PF_NUM)m_DatasArray[j])->RemoveAt(0,nPhysicalCount);
				break;
			case fString:
				((PF_STRING)m_DatasArray[j])->RemoveAt(0,nPhysicalCount);
				break;
			default:
				break;
			}
		}
        //修改目标页面状态等变量
		//pPageEmpty->SetPageState(pgNormal);
		pPageEmpty->m_bIsModify = true;

		
		//目标页而头部信息.
		pPageEmpty->m_lngFakeStart = m_lngFakeStart;
		//assert(pPageEmpty->m_lngFakeStart>0);
		pPageEmpty->m_lngLStartRecNo = m_lngLStartRecNo;
		pPageEmpty->SetLogicCount(nPhysicalCount);//移动的总数-其中删除的个数
		pPageEmpty->m_nOriginLoadCount = 0;//本页伪纪录个数 ===temp

		
		m_bIsModify = true;
		
        //源页面头部信息.
		// 现在的值 = 原来的值 + 移到他页的个数
		m_lngLStartRecNo = m_lngLStartRecNo + nPhysicalCount;//()内为已移动的逻辑纪录号的个数;
		// 现在的值 = 原来的值 - 移到他页的个数
		m_nLogicCount = m_nLogicCount - pPageEmpty->GetLogicCount();
		m_nOriginLoadCount;//本页伪纪录个数 不变

		
		//以下二个变量如果为负值,需调整.调整为第一个逻辑纪录的值.
		m_nCurNo -= pPageEmpty->m_nLogicCount;
		if(m_nCurNo<0)
			First();
		if (pPageEmpty->GetLogicCount())
		{
			m_pPages->m_PageArraySort.MoveToWorkArea(pPageEmpty); //12-30
		}

    }
////////////////////////////////////////////////////////////////////////////////////////
	//从页尾开始拷贝nPhysicalCount个纪录
	if(CopyFlags == FromEnd)
	{
		for(int i = m_nLogicCount-nPhysicalCount;i<m_nLogicCount;++i)
		{
			for(int j = 0;j<m_DatasArray.GetSize();j++)
			{  //将数据集中的数据地址赋到各字段的数据指针中
				CField * pField = m_pFields->FieldByIndex(j);				
				switch(pField->GetFieldType())
				{
				case fCurrency:
				case fBoolean:
				case fDate:
				case fInt:
				case fDouble:
					((PF_NUM)pPageEmpty->m_DatasArray[j])->Add(((PF_NUM)m_DatasArray[j])->GetAt(i));
					break;
				case fString:
					((PF_STRING)pPageEmpty->m_DatasArray[j])->Add(((PF_STRING)m_DatasArray[j])->GetAt(i));
					break;
				default:
					break;
				}
			}
		}

		//一次性删除
		for(int j = 0;j<m_DatasArray.GetSize();j++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(j);
			switch(pField->GetFieldType())
			{
				case fCurrency:
				case fBoolean:
				case fDate:
				case fInt:
				case fDouble:
				((PF_NUM)m_DatasArray[j])->RemoveAt(m_nLogicCount-nPhysicalCount,nPhysicalCount);
				break;
			case fString:
				((PF_STRING)m_DatasArray[j])->RemoveAt(m_nLogicCount-nPhysicalCount,nPhysicalCount);
				break;
			default:
				break;
			}
		}
		
		//修改目标页面状态等变量
		pPageEmpty->m_bIsModify = true;
		pPageEmpty->m_nOriginLoadCount = m_nOriginLoadCount;//转移到后面一页
		m_nOriginLoadCount = 0;
		
		//pPageEmpty->m_FState = dsBrowse;
		//源页面信息
		//m_lngFakeStart;  //不变
		//m_lngLStartRecNo;          //不变
		m_nLogicCount = m_nLogicCount - nPhysicalCount;
		//目标页面信息
		pPageEmpty->m_lngFakeStart = m_lngFakeStart ;
		//assert(pPageEmpty->m_lngFakeStart>0);
		pPageEmpty->m_lngLStartRecNo = m_lngLStartRecNo + m_nLogicCount;
		pPageEmpty->SetLogicCount(nPhysicalCount);
		
		m_bIsModify = true;
		//如果当前纪录被移动.调整为第一个逻辑纪录的值.
		if(m_nCurNo>=m_nLogicCount)
			First(); 

		if (pPageEmpty->GetLogicCount())
		{
			m_pPages->m_PageArraySort.MoveToWorkArea(pPageEmpty);
		}
    }
	this->HangFields(m_pPages->m_pFields);
	return true;

}

/***********************************************************************************
函数名称：
CPage::AdjustFakeRecNo
功能说明：
    根据传入的logicRecNo,fakeRecNo决定是否需要调整本页面，与本页所处状态无关

详细解释：
    由叶面集的UnLoad一个页时调用，它的偏移量 nFakeOffset 由卸载页面的unload方法返回
    页面集将调用各个页面的这个方法调整大纪录页的起始伪纪录号
    修改起始伪纪录号
出入参数：
    [in]logicRecNo：传入卸载页的逻辑纪录号,应为某页的逻辑起始纪录号
    [in]fakeRecNo:传入卸载页的伪纪录号,当逻辑记录号相同时再比较该参数,
	              连续删除几个页,会出现逻辑纪录号相同的情况.

    [in]nFakeOffset:逻辑纪录号与伪纪录号的偏移
返回类型：

制作：
    Eric2002-07-03
修改: 
************************************************************************************/
void CPage::AdjustFakeRecNo(int logicRecNo,int fakeRecNo, int nFakeOffset)
{
	
	if(m_lngLStartRecNo > logicRecNo) //(==时不改变)
	{  //如果此页的起始逻辑纪录号大于传入的纪录号,无论页面处在何种状态,都要改变起始伪纪录号.
		m_lngFakeStart = m_lngFakeStart+nFakeOffset;
		//assert(m_lngFakeStart>0);
	}
	if (m_lngLStartRecNo == logicRecNo)
	{//如果逻辑记录号相等,则再通过伪记录号判别
		if (m_lngFakeStart > fakeRecNo)
		{
			m_lngFakeStart = m_lngFakeStart+nFakeOffset;
			//assert(m_lngFakeStart>0);
		}
	}
	//assert(m_lngFakeStart>0);
}


/************************************************************************************
函数名称：
    CPage::Next
功能说明：
    逻辑移动纪录
详细解释：
出入参数：
返回类型：
    
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::Next()
{	

	//如果已到达逻辑末纪录,则返回.
	if(m_nCurNo == m_nLogicCount-1) 
	{
		return false;
		//assert(0);
	}

	m_nCurNo++;
	//物理纪录号发生改变

	BindFields();//===temp yh 可以不要
	return true;

//}}}END  2002-11-20 Eric
}

/************************************************************************************
函数名称：
    CPage::Prior
功能说明：
    逻辑移动纪录
详细解释：
出入参数：
返回类型：
    
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::Prior()
{

	//assert(m_nCurNo);//如果已到达逻辑首纪录,则返回.

	m_nCurNo--;

	BindFields();
	return true;

//}}}END  2002-11-20 Eric
}

/************************************************************************************
函数名称：
    CPage::MoveBy
功能说明：
    逻辑移动纪录
详细解释：
出入参数：
    [in]iSkipLen：步长
    [in]MoveFlags:动起始位置
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::MoveBy(int iSkipLen ,CFromPos MoveFlags)
{
	//移动到 CFromPos 指定的位置,同时检查参数
	switch(MoveFlags)
	{
	case FromBegin:
		{
			m_nCurNo = iSkipLen;
		}
		break;		
	}

	//恢复本页的绑定信息
	BindFields();
	return true;
}





/************************************************************************************
函数名称：
    CPage::First
功能说明：
    移动到逻辑第一条纪录
详细解释：
出入参数：
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::First()
{
	//移动到物理首纪录,如果其为删除状态,再向下移一条纪录.


	m_nCurNo = 0;

	BindFields();
	return true;

//}}}END  2002-11-20 Eric
}

/************************************************************************************
函数名称：
    CPage::Last
功能说明：
    移动到逻辑最后一条纪录
详细解释：
    移动到物理末纪录,如果其为删除状态,再向上移一条纪录.
出入参数：
返回类型：
    
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::Last()
{

	//移动到物理末纪录,如果其为删除状态,再向上移一条纪录.

	m_nCurNo = m_nLogicCount -1;
	//物理纪录号发生改变
	BindFields();

	return true;
//}}}END  2002-11-20 Eric

}




/************************************************************************************
函数名称：
    CPage::GetRecNo
功能说明：
    返回当前记录号
详细解释：
出入参数：
返回类型：
    int	:纪录号
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::GetRecNo()
{
	return m_nCurNo+m_lngLStartRecNo;
}

/************************************************************************************
函数名称：
   CPage::FieldByName 
功能说明：
    由字段名称得到字段
详细解释：
出入参数：
    [in]FieldName：字段名称
返回类型：
    CField*	:字段指针
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
CField* CPage::FieldByName(CTString FieldName)
{
	if(m_pFields)
	{
		CField * pField;
		pField = m_pFields->FieldByName(FieldName);
		return pField;
	}
	return 0;
}



/************************************************************************************
函数名称：

功能说明：
    设置页面为增加状态
详细解释：
    Append时,值先保存在NEW 出的缓冲区中
出入参数：
返回类型：
    void
制作：

修改: 
************************************************************************************/
#ifdef TEMP_Reserve
bool CPage::Append()
{
	if(m_pFields)
	{
		m_pFields->NewData();
	}
	return true;
}
bool CPage::Post()
{  /*
	if (m_pFields->GetLastVersion())//#_修改 2003-12-24 $ 14:22:30 zlq
		m_pFields->SetCurVersion();//08-26

	//CField * pTempField; //使用字段的方法操作数据
	int i=0;

	switch(m_FState)
	{
	case dsBrowse:
		break;
	case dsInsert:
		//向每个字段插入新值
		for( i = 0;i<m_DatasArray.GetSize();i++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(i);
			pField->pgInsertAt(m_DatasArray[i],m_nPhysicalNo);
		}//#_修改 2002-11-25 $ 17:39:16 zlq

		//修改纪录状态列表中信息
		m_bytePhysicalStateArray.InsertAt(m_nPhysicalNo,psAdd);
		m_OriginRecPosArray.InsertAt(m_nPhysicalNo,m_TempOriginPos);//#_修改 2002-12-26 $ 14:06:05 zlq
		m_LogNoArray.InsertAt(m_nCurNo,m_nPhysicalNo);
		for( i = m_nCurNo+1;i<m_LogNoArray.GetSize();i++)
		{
			m_LogNoArray[i]++;
		}


		if (GetPageState() == pgNormal)
		{//确保不是IO线程调用
			SetLogicCount(m_nLogicCount+1);//页面总逻辑个数
			m_bIsModify = true;
			
			if(GetPhysicalCount() >= m_pPages->GetMaxCountPerPage())//200
			{
				CFromPos CopyPos;
				if(m_nCurNo >= m_pPages->GetNormalCountPerPage())
					CopyPos = FromBegin;
				else
					CopyPos = FromEnd;
				CopyToOther(CopyPos,m_pPages->GetNormalCountPerPage());
			}		
		}
		BindFields();
		
		break;
	case dsAppend:
		for( i = 0;i<m_DatasArray.GetSize();i++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(i);
			pField->pgAdd(m_DatasArray[i]);
		}//#_修改 2002-11-25 $ 17:51:52 zlq

		//修改纪录状态列表中信息
		m_bytePhysicalStateArray.Add (psAdd);
		m_OriginRecPosArray.Add(m_TempOriginPos);//#_修改 2002-12-26 $ 14:06:05 zlq
		m_nCurNo++;//当前逻辑纪录号(每页重新从0开始编号.)
		m_nPhysicalNo =GetPhysicalCount()-1;
		m_LogNoArray.Add(m_nPhysicalNo);		

		if (GetPageState() == pgNormal)
		{//确保不是IO线程调用
			SetLogicCount(m_nLogicCount+1);//页面总逻辑个数
			m_bIsModify = true;

			if(GetPhysicalCount() >= m_pPages->GetMaxCountPerPage())
			{
				CopyToOther(FromBegin,m_pPages->GetNormalCountPerPage());
			}
		}
		BindFields();
		break;
	case dsEdit:
		for( i = 0;i<m_DatasArray.GetSize();i++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(i);
			//#_修改 2002-11-25 $ 18:11:55 zlq
			pField->pgSetAt(m_DatasArray[i],m_nPhysicalNo);
		}

		BindFields();

		if (m_bytePhysicalStateArray[m_nPhysicalNo] == psNoChange)//修改纪录状态列表中信息,原来添加的不能进入修改状态
			m_bytePhysicalStateArray[m_nPhysicalNo] = psModify;
		m_bIsModify = true;
		break;
	default:
		break;
	}
	m_FState = dsBrowse;*/
	return true;

//}}}END  2002-11-20 Eric
}
/************************************************************************************
函数名称：
    CPage::Edit
功能说明：
    设置页面为编辑状态
详细解释：
    EDIT时,值先保存在NEW 出的缓冲区中
出入参数：
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
bool CPage::Edit()
{/*	
	//往缓冲区中写入数据,一次写入一个字段.
	m_pFields->NewData();
	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		PF_NUM pDoubleField;
		switch(pField->GetFieldType())
		{
		case fInt:
			pField->GetData ((LPBYTE) &((*(PF_INT)m_DatasArray[i])[m_nPhysicalNo]));
			break;
		case fDouble:
			pDoubleField = (PF_NUM)m_DatasArray[i];
			pField->GetData ((LPBYTE) &((*pDoubleField)[m_nPhysicalNo]));
			break;
		case fString:
			pField->GetData ((LPBYTE) &((*(PF_STRING)m_DatasArray[i])[m_nPhysicalNo]));
			break;
		case fCurrency:
			pField->GetData ((LPBYTE) &((*(PF_CURRENCY)m_DatasArray[i])[m_nPhysicalNo]));
			break;
		case fBoolean:
			pField->GetData ((LPBYTE) &((*(PF_BOOL)m_DatasArray[i])[m_nPhysicalNo]));
			break;
		case fDate:
			pField->GetData ((LPBYTE) &((*(PF_DATE)m_DatasArray[i])[m_nPhysicalNo]));
			break;
		default:
			break;
		}
	}*/
	//m_FState = dsEdit;
	return true;
}


/************************************************************************************
函数名称：
    CPage::Insert
功能说明：
    设置页面为插入状态
详细解释：
出入参数：
返回类型：
    
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
bool CPage::Insert()
{
	if(m_pFields)
	{
		m_pFields->NewData();
		//m_FState = dsInsert;
	}
	return true;
}

bool CPage::Post(CStoreType nStoreType ,long lngPosition )
{
	SetOriginPos(nStoreType,lngPosition);
	return Post();	
}
#endif
/************************************************************************************
函数名称：
    CPage::Delete
功能说明：
    逻辑删除当前纪录
详细解释：
    如果不是新增的纪录,则仅做上删除标记.
	如果是新增的纪录,则从内存中删除.
出入参数：
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
bool CPage::_DeleteEx()
{
	if (m_pFields->GetLastVersion())//===ky  肯定时最新版本了
		m_pFields->SetCurVersion();

	{
		for(int i = 0;i<m_DatasArray.GetSize();i++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(i);
			pField->pg_RemoveAt(m_DatasArray[i],m_nCurNo);
		}

		m_nLogicCount--;
		if(m_nCurNo >= m_nLogicCount)
		{			
			m_nCurNo = m_nLogicCount-1;	
		} 

		m_bIsModify = true;
		if (m_nLogicCount == 0)
			return false;
		BindFields();
	}

	return true;
}





/************************************************************************************
函数名称：
    CPage::SetRecord
功能说明：
	设置当前条记录的所有的字段的数据信息到一个指定的缓冲区里
	数据组织按指示信息部分（所有字段，每个字段一字节）+数据�
	分（所有字段，可能某些字段也没有数据，前面能够表达时）
	返回数据长度
详细解释：
出入参数：
    [in,out]lpData：缓冲区指针
返回类型：
    int	: 数据长度
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
int CPage::_SetRecord(LPBYTE lpData)
{ 
	if(m_pFields)
	{
	    //BindFields();
		return (int)m_pFields->SaveDataToBuffer(lpData);  //
	}
	else
	{
		return false;
	}
}

/************************************************************************************
函数名称：
    CPage::GetRecord
功能说明：
    对当前的记录数据赋值操作
详细解释：
	（数据集在此之前应该通过处于编辑
	或添加状态，调用append,edit等方法），
	调用字段链表各字段的LoadDataFromBuffer
	返回是否装载成功，及有效缓冲区数据大小
    
出入参数：
    [in,out]lpData： 缓冲区
    [in,out]count：字节数
返回类型：
    bool :是否成功
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
bool CPage::GetRecord(LPBYTE lpData, int& count)
{
	LPBYTE lpDatatemp = lpData;
	count = 0;
	if (!GetRecord(lpDatatemp))
		return false;
	count = lpDatatemp - lpData;
	return true;
}

/************************************************************************************
函数名称：
    CPage::GetRecord
功能说明：
    对当前的记录数据赋值操作
详细解释：
	（数据集在此之前应该通过处于编辑
	或添加状态，调用append,edit等方法），并且修改指针
	调用字段链表各字段的LoadDataFromBuffer
出入参数：
    [in,out]lpData： 缓冲区指针
返回类型：
    bool:是否成功
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
bool CPage::GetRecord(LPBYTE& lpData)
{
	//如果此页不在 append edit Insert 状态,返回
	//if(m_FState != dsAppend&&m_FState != dsInsert&&m_FState != dsEdit)
	//	return false;


	if(m_pFields)
	{
	    //BindFields();
		int count;
		return m_pFields->LoadDataFromBuffer(lpData, count); 
	}
	else
	{
		return false;
	}
}



/************************************************************************************
函数名称：

功能说明：
    从内存中删除数据,只剩下字段链表
详细解释：
出入参数：
返回类型：
    void
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
void CPage::EmptyTable()
{
	//if (GetPageState()== pgNormal)
	//if (nStoreType == storeMemory)
	{//主线程调用
		m_pPages->m_pFields->GetBookmark();	 
		HangFields(m_pPages->m_pFields);
	}

	for( int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		pField->pg_RemoveAll(m_DatasArray[i]);
	}//#_修改 2002-11-26 $ 9:03:55 zlq


	m_bIsModify = false;   //本页是否改变过

	m_bReachEndError = false;
	
	//m_lngFakeStartRecNo=1; //本页起始伪纪录号
	//m_lngLStartRecNo = 1;  //本页起始逻辑纪录号
	//m_nLogicCount = 0;     //本页逻辑纪录号的个数 不用函数SetLogicCount()
	//m_nOriginLoadCount = 50;//本页伪纪录个数
	
	m_nCurNo = -1;			//当前逻辑纪录号 
	//m_pFields =0;		   //指向CFields 的指针
	
	
	//if (GetPageState()== pgNormal)
	//if (nStoreType == storeMemory)	
	{//主线程调用
		m_pPages->m_pFields->GotoBookmark();
	}

}
/************************************************************************************
函数名称：
    CPage::IsLogiclEmpty
功能说明：
    是否逻辑空页
详细解释：
    当页面处于正常状态时,是否是逻辑空页.即全部是删除的数据,这样这个启动记录号才有用
出入参数：
返回类型：
    bool
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
bool CPage::IsLogicalEmpty()
{
	if(m_nLogicCount == 0)
		return true;
	else
		return false;
}

/************************************************************************************
功能说明：
    是否是物理空页
详细解释：
    没有物理纪录.
出入参数：
返回类型：
    
制作：
    Eric2002-07-03
修改: 
************************************************************************************/
bool CPage::IsPhysicalEmpty()
{
	{//主线程调用
		m_pPages->m_pFields->GetBookmark();	 
		HangFields(m_pPages->m_pFields);
	}	
	bool brst = true;
	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		int ArraySize = 0;
		if (pField->GetFieldType() == fString)
			ArraySize =((PF_STRING)m_DatasArray[i])->GetSize();
		else
			ArraySize =((PF_NUM)m_DatasArray[i])->GetSize();
		////assert(ArraySize ==m_nLogicCount);
		if (ArraySize >0)
			brst = false;
	}	
	{//主线程调用
		m_pPages->m_pFields->GotoBookmark();
	}
	return brst;
	/*
	if (m_nLogicCount==0 && m_nOriginLoadCount==0)
		return true;
	else
		return false;
	
	if(m_bytePhysicalStateArray.GetSize() == 0)
		return true;
	else
		return false;*/
}
/************************************************************************************
函数名称：
    CPage::GetCurVersion
功能说明：
    得到当前页面的版本号
详细解释：
出入参数：
返回类型：
    
制作：
    Eric2002-07-12
修改: 
************************************************************************************/
int CPage::GetCurVersion()
{
	 return m_pFields->GetCurVersion();
}
/************************************************************************************
函数名称：
	CPage::AdjustLogicRecNo
功能说明：
    根据传入的logicRecNo决定是否需要调整所有页面，与本页所处状态无关
详细解释：
    页面集将调用各个页面的这个方法调整大纪录页的起始逻辑纪录号
    修改起始逻辑纪录号
出入参数：
    [in]logicRecNo：传入的逻辑纪录号,应为某页的逻辑起始纪录号
	[in]nFakeOffset:逻辑纪录号的偏移
返回类型：
    
制作：
    Eric2002-07-24
修改: 
************************************************************************************/

void CPage::AdjustLogicRecNo(int logicRecNo,int nOffset)
{
	if(m_lngLStartRecNo > logicRecNo) //(==时不改变)
	{  //如果此页的起始逻辑纪录号大于传入的纪录号,无论页面处在何种状态,都要改变起始伪纪录号.
		m_lngLStartRecNo += nOffset;
	}
}
/************************************************************************************
函数名称：
	CPage::InsertField
功能说明：
	插入一个字段
详细解释：

出入参数：
	[in]nIndex	：插入的位置
	[in]pNewField:插入的字段
返回类型：

制作：
	Eric 2002-9-4
修改:

************************************************************************************/
void CPage::InsertField(int nIndex,CField* pNewField)
{

	PF_NUM                pDouble;    
	PF_STRING                pString;    
     

	FieldNUM                F_Double;
	FieldString                        F_Str;
	
	F_Double =   NULLValue;

   
	
	int nPhyCount = m_nLogicCount ;//m_bytePhysicalStateArray.GetSize();
	
	int i;
	switch(pNewField->GetFieldType())
	{
	case fInt:
	case fDate:
	case fBoolean:
	case fCurrency:
	case fDouble:
		pDouble = new F_NUM;
		for(i = 0;i<nPhyCount;i++)
		{
			pDouble->Add(F_Double);
		}
		m_DatasArray.InsertAt(nIndex,pDouble);
		break;
	case fString:
		pString = new F_STRING;
		for(i = 0;i<nPhyCount;i++)
		{
			pString->Add(F_Str);
		}
		m_DatasArray.InsertAt(nIndex,pString);
		break;

	default:
		break;
	}

}
/************************************************************************************
函数名称：
	CPage::AddField
功能说明：
	增加一个字段
详细解释：

出入参数：
	[in] pNewField :新增的字段的指针
返回类型：

制作：
	Eric 2002-9-4
修改:

************************************************************************************/
void CPage::AddField(CField* pNewField)
{ 

  
		PF_NUM		pDouble;    
		PF_STRING		pString;    

		FieldNUM		F_Double;
		FieldString			F_Str;

		//F_Double.Data[0] = 0xFE;
		F_Double = NULLValue;

		F_Str ="";
								  
		int nPhyCount = m_nLogicCount;//m_bytePhysicalStateArray.GetSize();

		int i;
		switch(pNewField->GetFieldType())
		{
		case fInt:
		case fDate:
		case fBoolean:
		case fCurrency:
		case fDouble:
			pDouble = new F_NUM;
			pDouble->SetSize(nPhyCount,IncreaseSize);
			for(i = 0;i<nPhyCount;i++)
				pDouble->SetAt(i,F_Double);
 			m_DatasArray.Add(pDouble);
			break;
		case fString:
			pString = new F_STRING;
			pString->SetSize(nPhyCount,IncreaseSize);
			for(i = 0;i<nPhyCount;i++)
				pString->SetAt(i,F_Str);
   			m_DatasArray.Add(pString);
			break;

		default:
			break;
		}
}

/************************************************************************************
函数名称：
	CPage::DeleteField
功能说明：
	删除一个字段
详细解释：

出入参数：
	[in]nIndex	 ：删除的位置
	[in]pOldField  ：删除的字段.此时Fields里已没有此字段,所以由外面传进来
返回类型：

制作：
	Eric 2002-9-4
修改:

************************************************************************************/
void CPage::DeleteField(int nIndex,CField* pOldField)
{

	if(nIndex >m_DatasArray.GetSize()||nIndex<0)
	{
		//assert(0);
		return ;
	}


	PF_NUM	pDouble;    
	PF_STRING	pString;    
 
	FieldNUM F_Double;
	F_Double= NULLValue;
	//int i;
	//CField *pField = m_pFields->FieldByIndex(nIndex);
	switch(pOldField->GetFieldType())
	{
	case fInt:
	case fDate:
	case fBoolean:
	case fCurrency:
	case fDouble:
		pDouble = (PF_NUM)(m_DatasArray[nIndex]);
		pDouble->RemoveAll();
		delete pDouble;
		m_DatasArray.RemoveAt(nIndex);
		break;
	case fString:
		pString = (PF_STRING)(m_DatasArray[nIndex]);
		pString->RemoveAll();
		delete pString;
		m_DatasArray.RemoveAt(nIndex);
		break;

	default:
		break;
	}

	if(m_DatasArray.GetSize() == 0)
	{
		m_bIsModify = false;   //本页是否改变过


		m_lngFakeStart=1; //本页起始伪纪录号
		m_lngLStartRecNo = 1;  //本页起始逻辑纪录号
		SetLogicCount(0);      //本页逻辑纪录号的个数
		m_nOriginLoadCount = 0;//本页伪纪录个数

		m_nCurNo = 0;			//当前逻辑纪录号 
	}
}
/************************************************************************************
函数名称：
	CPage::ModifyField
功能说明：
	修改一个字段
详细解释：

出入参数：
	[in] nIndex	 ：要修改的位置
	[in] pOldField	：要修改的字段
	[in] pNewField：  将换为此字段
返回类型：

制作：
	Eric 2002-9-4
修改:

************************************************************************************/
void CPage::ModifyField(int nIndex,CField* pOldField, CField* pNewField)
{
	if(nIndex > m_DatasArray.GetSize() || nIndex < 0)
	{
		//assert(0);
		return ;
	}

	PF_NUM		pInt;   
	PF_NUM	pDouble;    
	PF_STRING	pString;    
	PF_STRING	pString_;    
	PF_NUM	pCurrency;  
	PF_NUM		pBool;      
     
	PF_NUM		pDate;  

	FieldNUM F_Double;
	F_Double = NULLValue;
	int i;
	pNewField->NewData();

	switch(pOldField->GetFieldType())
	{
	case fInt://新字段
		pInt = (PF_NUM)(m_DatasArray[nIndex]);
		switch(pNewField->GetFieldType())
		{
			case fInt:
				return;//实际上没有修改
			case fDouble:
				pDouble =  new F_NUM;
				pDouble->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDouble);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pInt)[i]);
					TransFieldValue(pNewField,pOldField);
					pDouble->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fString:
				pString = new F_STRING;
				pString->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pString);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pInt)[i]);
					TransFieldValue(pNewField,pOldField);
					FieldString strTemp;
					FieldString * pTemp = (FieldString *)pNewField->m_pValue;
					strTemp	= *pTemp ;
					pString->Add(strTemp);
				}
				break;
			case fCurrency:
				pCurrency = new F_NUM;
				pCurrency->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pCurrency);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pInt)[i]);
					TransFieldValue(pNewField,pOldField);
					pCurrency->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fBoolean:
				pBool = new F_NUM;
				pBool->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pBool);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pInt)[i]);
					TransFieldValue(pNewField,pOldField);
					pBool->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;

			case fDate:
				pDate = new F_NUM;
				pDate->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDate);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pInt)[i]);
					TransFieldValue(pNewField,pOldField);
					pDate->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			default:
				break;	
		}
		delete pInt;

		break;
	case fDouble://新字段
		pDouble = (PF_NUM)(m_DatasArray[nIndex]);
		switch(pNewField->GetFieldType())
		{
			case fInt:
				pInt = new F_NUM;
				pInt->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pInt);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pDouble)[i]);
					TransFieldValue(pNewField,pOldField);
					pInt->Add(*((FieldNUM*)pNewField->m_pValue));
				} 

				break;
			case fDouble:
				return;//未改变
			case fString:
				pString = new F_STRING;
				pString->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pString);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pDouble)[i]);
					TransFieldValue(pNewField,pOldField);
					FieldString strTemp;
					FieldString * pTemp = (FieldString *)pNewField->m_pValue;
					strTemp	= *pTemp;
					pString->Add(strTemp);
				}
				break;
			case fCurrency:
				pCurrency = new F_NUM;
				pCurrency->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pCurrency);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pDouble)[i]);
					TransFieldValue(pNewField,pOldField);
					pCurrency->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fBoolean:
				pBool = new F_NUM;
				pBool->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pBool);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue =(LPBYTE) &((*pDouble)[i]);
					TransFieldValue(pNewField,pOldField);
					pBool->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;

			case fDate:
				pDate = new F_NUM;
				pDate->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDate);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pDouble)[i]);
					TransFieldValue(pNewField,pOldField);
					pDate->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			default:
				break;	
		}
		delete pDouble;

		break;
	case fString://新字段
		pString = (PF_STRING)(m_DatasArray[nIndex]);
		switch(pNewField->GetFieldType())
		{
			case fInt:
				pInt = new F_NUM;
				pInt->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pInt);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pString) [i]);
					TransFieldValue(pNewField,pOldField);
					pInt->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fDouble:
				pDouble =  new F_NUM;
				pDouble->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDouble);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pString) [i]);
					TransFieldValue(pNewField,pOldField);
					pDouble->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fString:
				pString_ = new F_STRING;
				pString_->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pString_);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pString) [i]);
					TransFieldValue(pNewField,pOldField);
					FieldString strTemp;
					FieldString * pTemp = (FieldString *)pNewField->m_pValue;
					strTemp	= *pTemp;
					pString_->Add(strTemp);
				}
				break;				
				//return;//未改变
			case fCurrency:
				pCurrency = new F_NUM;
				pCurrency->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pCurrency);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pString) [i]);
					TransFieldValue(pNewField,pOldField);
					pCurrency->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fBoolean:
				pBool = new F_NUM;
				pBool->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pBool);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pString) [i]);
					TransFieldValue(pNewField,pOldField);
					pBool->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;

			case fDate:
				pDate = new F_NUM;
				pDate->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDate);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &((*pString) [i]);
					TransFieldValue(pNewField,pOldField);
					pDate->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			default:
				break;	
		}
		delete pString;

		break;
	case fCurrency://新字段
		pCurrency = (PF_NUM)(m_DatasArray[nIndex]);
		switch(pNewField->GetFieldType())
		{
			case fInt:
				pInt = new F_NUM;
				pInt->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pInt);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pCurrency)[i];
					TransFieldValue(pNewField,pOldField);
					pInt->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;//实际上没有修改
			case fDouble:
				pDouble =  new F_NUM;
				pDouble->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDouble);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pCurrency)[i];
					TransFieldValue(pNewField,pOldField);
					pDouble->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fString:
				pString = new F_STRING;
				pString->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pString);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pCurrency)[i];
					TransFieldValue(pNewField,pOldField);
					pString->Add(*((FieldString*)pNewField->m_pValue));
				}
				break;
			case fCurrency:
				return;//未改变
			case fBoolean:
				pBool = new F_NUM;
				pBool->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pBool);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pCurrency)[i];
					TransFieldValue(pNewField,pOldField);
					pBool->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fDate:
				pDate = new F_NUM;
				pDate->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDate);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pCurrency)[i];
					TransFieldValue(pNewField,pOldField);
					pDate->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			default:
				break;	
		}
		delete pCurrency;

		break;
	case fBoolean://新字段
		pBool = (PF_NUM)(m_DatasArray[nIndex]);
		switch(pNewField->GetFieldType())
		{
			case fInt:
				pInt = new F_NUM;
				pInt->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pInt);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pBool)[i];
					TransFieldValue(pNewField,pOldField);
					pInt->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;//实际上没有修改
			case fDouble:
				pDouble =  new F_NUM;
				pDouble->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDouble);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pBool)[i];
					TransFieldValue(pNewField,pOldField);
					pDouble->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fString:
				pString = new F_STRING;
				pString->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pString);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pBool)[i];
					TransFieldValue(pNewField,pOldField);

					pString->Add(*((FieldString*)pNewField->m_pValue));
				}
				break;
			case fCurrency:
				pCurrency = new F_NUM;
				pCurrency->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pCurrency);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pBool)[i];
					TransFieldValue(pNewField,pOldField);
					pCurrency->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fBoolean:
				return;//未改变

			case fDate:
				pDate = new F_NUM;
				pDate->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDate);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pBool)[i];
					TransFieldValue(pNewField,pOldField);
					pDate->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			default:
				break;	
		}
		delete pBool;

		break;

	case fDate://新字段
		pDate = (PF_NUM)(m_DatasArray[nIndex]);
		switch(pNewField->GetFieldType())
		{
			case fInt:
				pInt = new F_NUM;
				pInt->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pInt);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pDate)[i];
					TransFieldValue(pNewField,pOldField);
					pInt->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fDouble:
				pDouble =  new F_NUM;
				pDouble->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pDouble);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pDate)[i];
					TransFieldValue(pNewField,pOldField);
					pDouble->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fString:
				pString = new F_STRING;
				pString->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pString);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pDate)[i];
					TransFieldValue(pNewField,pOldField);
					FieldString strTemp;
					FieldString * pTemp = (FieldString *)pNewField->m_pValue;
					strTemp	= *pTemp;
					pString->Add(strTemp);
				}
				break;
			case fCurrency:
				pCurrency = new F_NUM;
				pCurrency->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pCurrency);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pDate)[i];
					TransFieldValue(pNewField,pOldField);
					pCurrency->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;
			case fBoolean:
				pBool = new F_NUM;
				pBool->SetSize(0,IncreaseSize);
				m_DatasArray.SetAt(nIndex,pBool);
				for(i = 0;i<GetLogicCount();i++)
				{
					pOldField->m_pValue = (LPBYTE) &(*pDate)[i];
					TransFieldValue(pNewField,pOldField);
					pBool->Add(*((FieldNUM*)pNewField->m_pValue));
				} 
				break;

			case fDate:
				return;//未改变
			default:
				break;	
		}
		delete pDate;

		break;
	default:
		break;
	}

	pNewField->DeleteData();//

	if(m_DatasArray.GetSize() == 0)
	{
		m_bIsModify = false;   //本页是否改变过

		m_lngFakeStart=1; //本页起始伪纪录号
		m_lngLStartRecNo = 1;  //本页起始逻辑纪录号
		SetLogicCount(0);      //本页逻辑纪录号的个数
		m_nOriginLoadCount = 0;//本页伪纪录个数

		m_nCurNo = 0;			//当前逻辑纪录号 
	//}}}END  2002-11-22 Eric
	}
}

/************************************************************************************
函数名称：
	CPage::TransFieldValue
功能说明：
	不同字段类型的字段间值的转换.
详细解释：

出入参数：
	[in]pNewField：新的字段
	[in]pOldField ：旧的字段
返回类型：

制作：
	Eric 2002-9-4
修改:

************************************************************************************/
void CPage::TransFieldValue(CField* pNewField, CField* pOldField)
{
	if(pOldField->IsNull())
	{
		pNewField->SetNull();
		return;
	}

	if(pNewField->GetFieldType()==fString)
	{
	    pNewField->SetAsString(pOldField->GetAsString());
		return;
	}

	pNewField->SetNull();

	switch(pOldField->GetFieldType())
	{
	case fString :
		pNewField->SetAsString(pOldField->GetAsString());
		break;
	case fInt :
		pNewField->SetAsInteger(pOldField->GetAsInteger());
		break;
	case fBoolean :
		pNewField->SetAsBool(pOldField->GetAsBool());
		break;
	case fDate :
		pNewField->SetAsDateTime(&pOldField->GetAsDateTime());
		break;

	case fCurrency :
		pNewField->SetAsCurrency(pOldField->GetAsCurrency());
		break;
	case fDouble:
		pNewField->SetAsDouble(pOldField->GetAsDouble());
		break;
	}
}
/************************************************************************************
函数名称：
	CPage::SetCurVersion
功能说明：
	设置本页面的当前版本
详细解释：

出入参数：
	[in] nVersion :版本号
返回类型：

制作：
	Eric 2002-9-4
修改:

************************************************************************************/
void CPage::SetCurVersion(int nVersion)
{
	 m_pFields->SetCurVersion(nVersion);
}
/************************************************************************************
函数名称：
   CPage::GetNewestVersion
功能说明：
	得到最新的版本号
详细解释：

出入参数：
返回类型：
	int : 版本号
制作：
	Eric 2002-9-4
修改:

************************************************************************************/
int CPage::GetNewestVersion()
{
	return m_pFields->GetLastVersion();
}


/************************************************************************************
函数名称：

功能说明：

详细解释：

出入参数：
	[in,out]
返回类型：

制作：
	Eric 2002-11-20
修改:

************************************************************************************/
void CPage::IniFieldCnt(CFields *pFields)
{
	int nFieldCount = pFields->GetFieldCount();
	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = pFields->FieldByIndex(i);
		pField->pg_RemoveAll(m_DatasArray[i]);
	}

	m_DatasArray.RemoveAll();


	for(int iFieldIndex = 0;iFieldIndex < nFieldCount;iFieldIndex++	)
	{
		CField * pField = pFields->FieldByIndex(iFieldIndex);
		m_DatasArray.Add(pField->pg_New());
	}//#_修改 2002-11-26 $ 9:38:29 zlq
}


void CPage::MoveField(int nFrom, int nTo)
{
	if (nFrom == nTo)
		return;

	if (nFrom < nTo)
		nTo--;//如果移动到后面,则先减位置号
	
	void* value = m_DatasArray[nFrom] ;
	m_DatasArray.RemoveAt(nFrom);//从老位置去掉
	m_DatasArray.InsertAt(nTo,value);//移动到新位置	
}

bool CPage::_AppendIn(CStoreType nStoreType, long lngPosition)
{//内外都调用CPages::_Append()   CFileOperation::ReadRecords()
	int nCurVersion = m_pFields->GetCurVersion();
	m_pFields->SetCurVersion();

	{//case dsAppend:
		
		for(int i = 0;i<m_DatasArray.GetSize();i++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(i);
			pField->pg_Add(m_DatasArray[i]);
			
		}
		m_nCurNo++;//当前逻辑纪录号(每页重新从0开始编号.)

		BindFields();
	}
	
	m_pFields->SetCurVersion(nCurVersion);
	return true;
}
bool CPage::_AppendInBatch( int nCount)
{//内外都调用CPages::_Append()   CFileOperation::ReadRecords()
	int nCurVersion = m_pFields->GetCurVersion();
	m_pFields->SetCurVersion();//升到最新版本

	m_LastFieldsAry.RemoveAll();
	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		pField->pg_Add(m_DatasArray[i],nCount);
		m_LastFieldsAry.Add(pField);
	}
	//m_nCurNo++;//当前逻辑纪录号(每页重新从0开始编号.)
	
	//BindFields();

	
	m_pFields->SetCurVersion(nCurVersion);
	return true;
}
bool CPage::_AppendEx()
{//内外都调用CPages::_Append()   CFileOperation::ReadRecords()
	if (m_pFields->GetLastVersion())//===ky  肯定时最新版本了 ===temp 可以消除
		m_pFields->SetCurVersion();

	{//case dsAppend:
		for(int i = 0;i<m_DatasArray.GetSize();i++)
		{  //将数据集中的数据地址赋到各字段的数据指针中
			CField * pField = m_pFields->FieldByIndex(i);
			pField->pg_Add(m_DatasArray[i]);
		}

		m_nCurNo++;//当前逻辑纪录号(每页重新从0开始编号.)
		m_nLogicCount++;
		m_bIsModify = true;
		
		if(GetLogicCount() >= m_pPages->GetMaxCountPerPage())
		{
			CopyToOther(FromBegin,m_pPages->GetNormalCountPerPage());
		}

		BindFields();
		//break;
	}

	return true;
}
bool CPage::_Edit()
{
	m_bIsModify = true;
	return true;
}

bool CPage::_InsertEx()
{
	if (m_pFields->GetLastVersion())
		m_pFields->SetCurVersion();

	//向每个字段插入新值
	for(int  i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		pField->pg_InsertAt(m_DatasArray[i],m_nCurNo);
	}
	
	m_nLogicCount++;
	m_bIsModify = true;
	
	if(m_nLogicCount >= m_pPages->GetMaxCountPerPage())//200
	{
		CFromPos CopyPos;
		if(m_nCurNo >= m_pPages->GetNormalCountPerPage())
			CopyPos = FromBegin;
		else
			CopyPos = FromEnd;
		CopyToOther(CopyPos,m_pPages->GetNormalCountPerPage());
	}	
		
	BindFields();
	return true;
}

void CPage::PrintCurLine()//===temp
{
#ifdef tl_debug	
	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = m_pFields->FieldByIndex(i);
		int ArraySize = 0;
		if (pField->GetFieldType() == fString)
			ArraySize =((PF_STRING)m_DatasArray[i])->GetSize();
		else
			ArraySize =((PF_NUM)m_DatasArray[i])->GetSize();
		//assert(ArraySize ==m_nLogicCount);
	}	

	for ( i=0;i<m_pFields->GetFieldCount();i++)
	{
		//===temp TRACE(m_pFields->FieldByIndex(i)->GetAsString()+"\t");
	}
	//===temp TRACE("\r\n");
#endif	
}

bool CPage::_SetRec(int nRelativePos)
{
	if (nRelativePos <0 || nRelativePos>=m_nLogicCount)
		return false;
	m_nCurNo = nRelativePos;
	BindFields();
	return true;
}

bool CPage::_SetRecLast(int nRelativePos)
{
	if (nRelativePos <0 || nRelativePos>=m_nLogicCount)
		return false;
	m_nCurNo = nRelativePos;
	for(int i = 0;i<m_DatasArray.GetSize();i++)
	{  //将数据集中的数据地址赋到各字段的数据指针中
		CField * pField = (CField *)m_LastFieldsAry.GetAt(i);
		pField->m_pValue = pField->pg_GetAt(m_DatasArray[i],m_nCurNo);

	}
	return true;
}
