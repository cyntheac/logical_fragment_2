BEGIN_IMPLEMENT_FN0(INNER(DocLines), CString, RunAddToUnderloadOrders, A)
	RunAddToUnderloadOrders(true);
END_IMPLEMENT_FNC(return CString();)

BEGIN_IMPLEMENT_FN1(INNER(DocLines), CString, RunAddToUnderloadOrders, B, bool bCheckOrderStatus)

	if(!IsAddToUnderloadOrders())
		return "";
	if (GetDocument().GetTransaction().IsSetOutcomeInvisible())
		return "";


	COleVariant size[] = { get_No_of_lines((long)CELLS_RECORD) };
	CIBitBuffer bbCells(1, size);
	bbCells.Clear(0, size);
	COleVariant size2[] = { get_No_of_lines((long)REGISTERS_RECORD) };
	CIBitBuffer bbRegisters(1, size2);
	bbRegisters.Clear(0, size);
	CBitBuffer bbGoodsItems(get_No_of_lines((long)GOODS_ITEM_RECORD), NULL);

	INNER(DocLines)* r_DocLines = &GetDocument().GetDocLines();
	REC_LOOP(r_DocLines)
	{
		if (r_DocLines->GetPalletIndex() == GetPalletIndex())
		{
			if (r_DocLines->IsGoodsLoaded())
			{
				bbGoodsItems.SetBit(r_DocLines->GetGoodsItemRef(), true);
				COleVariant argRegisterRef[] = { r_DocLines->GetRegisterToRef() };
				bbRegisters.HewItem(1, argRegisterRef);
				COleVariant argCellRef[] = { r_DocLines->GetCellToRef() };
				bbCells.HewItem(1, argCellRef);
			}
			else
				return "";
		}
	}

	CMap<line_type, line_type, line_type, line_type> mapUnderloadBatches;
	CIArray arrOrders;
	CArray<COleVariant, COleVariant&> *pArr = arrOrders.GetData();

	PROXY(OrdersBatch) r_OrdersBatch(dsDB);
	PROXY(Orders) r_Order(dsDB);
	PROXY(OrdersLines) r_OrdersLines(dsDB);

	r_OrdersLines->SetSkipMode();
	r_OrdersLines->GetOrderRefFld().MustBeRefNE(NULL_REF);
	r_OrdersLines->GetOrdersBatchRefFld().MustBeRefNE(NULL_REF);
	r_OrdersLines->GetFld("@Пачка заказов")->GetRefFld("@Склад")->MustBeRefEQ(GetDocument().GetWarehouseToRef());
	if (GetDocument().GetDocumentTypeRef() == DOCUMENT_TYPE_INCOMG)
		r_OrdersLines->GetFld("@Пачка заказов")->GetRefFld("@Организация")->MustBeRefEQ(GetDocument().GetVendorToRef());
	else
		r_OrdersLines->GetFld("@Пачка заказов")->GetRefFld("@Организация")->MustBeRefEQ(GetDocument().GetVendorRef());
	if (bCheckOrderStatus)
		r_OrdersLines->GetFld("@Заказ")->GetRefFld("Признаки")->MustBeBitSet(r_Order->GetMovingToPickingBit());
	r_OrdersLines->GetFld("@Заказ")->GetRefFld("Признаки")->MustBeBitClr(r_Order->GetHasLeftWarehouseBit());
	r_OrdersLines->GetGoodsItemRefFld().MustBeRefInBitBuffer(&bbGoodsItems);
	r_OrdersLines->MustBeValid();
	REC_LOOP(r_OrdersLines)
	{
		if (_GT_(r_OrdersLines->CalcUnderloadQty(), 0))
		{
			mapUnderloadBatches.SetAt(r_OrdersLines->GetOrdersBatchRef(), r_OrdersLines->GetOrdersBatchRef());
			pArr->Add(COleVariant(r_OrdersLines->GetOrderRef()));
		}
	}

	CIOperationContext context;
	context.SetNeedCheckAccessibility(TRUE);

	COleVariant args[] = { "Фильтр по ячейкам" };
	bbCells.AddToCache(1, args);
	COleVariant args2[] = { "Фильтр по регистрам" };
	bbRegisters.AddToCache(1, args2);

	line_type lKey;
	for (POSITION pos = mapUnderloadBatches.GetStartPosition(); pos;)
	{
		lKey = NULL_REF;
		line_type lValue = 0;
		mapUnderloadBatches.GetNextAssoc(pos, lKey, lValue);
		r_OrdersBatch->SetLine(lKey);

		if (!r_OrdersBatch->IsValidLine())
			continue;

		INNER(Document)* r_PPU = &r_OrdersBatch->GetPalletPickingUp();
		r_PPU->SetLastLine();
		line_type lLastPPULine = r_PPU->GetLine();

		long batch_result = r_OrdersBatch->MakePalletPickingUpForOrders(&arrOrders, true);
		if (batch_result != ppurOK)
			continue;

		// найдём новую подборку
		r_PPU->SetLine(lLastPPULine);
		if (!r_PPU->IsValidLine())
			r_PPU->SetFirstLine();
		else
			r_PPU->SetNextLine();
		if (!r_PPU->IsValidLine())
			continue;

		// Ставим строки новой подборки в буфер, чтобы создались новые приказы
		INNER(DocLines)* r_DocLine = &r_PPU->GetDocLines();
		REC_LOOP(r_DocLine)
		{
			if (r_DocLine->IsGoodsInBuffer() || r_DocLine->IsGoodsLoaded())
				continue;
			CString csResult = r_DocLine->MoveSrcToBuf(1, &context);
		}
		r_PPU->WriteHistoryModificationDocuments();
	}

	bbCells.RemoveFromCache(1, args);
	bbRegisters.RemoveFromCache(1, args2);

END_IMPLEMENT_FNC(return CString();)
