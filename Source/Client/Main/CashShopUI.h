#pragma once

#include "ProtocolDefines.h"

#define CASHSHOP_MAX_PRODUCT 64

struct PMSG_CASHSHOP_INFO_REQ
{
	PSBMSG_HEAD header; // C1:98:11
};

struct PMSG_CASHSHOP_ENTRY
{
	BYTE type;          // 0: item, 1: VIP days
	WORD item;          // GET_ITEM(section, index)
	BYTE level;
	BYTE category;      // 1: Pets/Mounts, 2: Jewels/Items, 3: Event Tickets, 4: VIP Services
	DWORD price;
	char name[32];
	char description[60];
};

struct PMSG_CASHSHOP_LIST_ANS
{
	PSWMSG_HEAD header; // C2:98:11
	BYTE enable;
	DWORD wcoin;
	BYTE count;
};

struct PMSG_CASHSHOP_BUY_REQ
{
	PSBMSG_HEAD header; // C1:98:10
	BYTE product;       // 1-based product index
	BYTE param;
};

struct PMSG_CASHSHOP_BUY_ANS
{
	PSBMSG_HEAD header; // C1:98:10
	BYTE result;
	BYTE product;
	DWORD wcoin;
};

struct PMSG_SEPAY_INFO
{
	PSBMSG_HEAD header; // C1:99:30
	BYTE enable;
	DWORD rateVnd;
	DWORD rateWcoin;
	DWORD minAmount;
	char bankCode[16];
	char bankAccount[32];
	char bankOwner[48];
	char account[16];
};

class CCashShopUI
{
public:
	CCashShopUI();
	~CCashShopUI();

	void LoadImages();
	void Toggle();
	void Open();
	void Close() { this->m_Open = false; }
	bool IsOpen() const { return this->m_Open; }

	void OnListRecv(BYTE* lpMsg);
	void OnBuyRecv(BYTE* lpMsg);
	void OnSePayInfoRecv(BYTE* lpMsg);

	void Render();
	void UpdateMouse();

private:
	void RequestInfo();
	void RequestBuy(BYTE productIdx, BYTE param = 0);
	void StartQrDownload(DWORD amount);
	void UploadPendingQr();
	void RenderTopUpModal();
	bool HandleTopUpMouse();
	static DWORD WINAPI QrDownloadThread(LPVOID lpParam);

	bool m_Open;
	bool m_Enable;
	int m_Category; // 0: All, 2: Jewels, 4: VIP & Services
	int m_Page;     // 0-based
	DWORD m_WCoin;
	int m_Count;
	PMSG_CASHSHOP_ENTRY m_Products[CASHSHOP_MAX_PRODUCT];

	DWORD m_WaitTick;
	DWORD m_StatusTick;
	bool m_StatusGood;
	char m_StatusText[96];

	int m_ConfirmProductIdx; // -1 = none, 0..m_Count-1 = pending confirm
	int m_SelectedClass;     // 0: DW, 1: DK, 2: ELF, 3: MG

	unsigned int m_TexShopCS;

	// SePay QR Top-Up state
	bool m_HasSePayInfo;
	PMSG_SEPAY_INFO m_SePayInfo;
	bool m_SePayOpen;
	DWORD m_SePayAmount;
	DWORD m_SePayPollTick;
	DWORD m_SePayPrevWCoin;
	unsigned int m_TexQr;
	CRITICAL_SECTION m_QrLock;
	unsigned char* m_QrPixels;
	int m_QrW;
	int m_QrH;
	bool m_QrReady;
	bool m_QrLoading;
	bool m_QrError;
	volatile long m_QrSeq;
};

extern CCashShopUI gCashShopUI;
