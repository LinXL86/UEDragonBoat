// Fill out your copyright notice in the Description page of Project Settings.

#include "Datamanagement.h"

ADatamanagement::ADatamanagement()
{
	PrimaryActorTick.bCanEverTick = true;
	SelectedTileIndex = -1;
	PendingSwapIndexA = -1;
	PendingSwapIndexB = -1;
	GameState = EMatch3State::Idle;
}

void ADatamanagement::BeginPlay()
{
	Super::BeginPlay();
	InitializeGame();
}

void ADatamanagement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ========================================
// 核心交换逻辑
// ========================================

bool ADatamanagement::TrySwap(int32 IndexA, int32 IndexB)
{
	// 只在空闲状态允许新的交换操作
	if (GameState != EMatch3State::Idle)
		return false;

	// 1. 预执行交换，检查数据
	OrbGrid.Swap(IndexA, IndexB);
	
	// 2. 检查交换后是否产生任何匹配
	bool bValidMove = HasMatch(); 

	if (bValidMove)
	{
		// 有效移动：保留交换数据，进入交换动画状态
		StartSwap(IndexA, IndexB);
		return true;
	}
	else
	{
		// 无效移动：恢复原数据，播放失败动画
		OrbGrid.Swap(IndexA, IndexB);
		PendingSwapIndexA = IndexA;
		PendingSwapIndexB = IndexB;
		RevertSwap(IndexA, IndexB);
		return false;
	}
}

void ADatamanagement::AdvanceGameState()
{
	UE_LOG(LogTemp, Log, TEXT("AdvanceGameState called, Current State: %d"), (int32)GameState);
	
	switch (GameState)
	{
	case EMatch3State::Swapping:
		// 交换动画完成 -> 开始检查匹配
		UE_LOG(LogTemp, Log, TEXT("  -> Swapping finished, checking matches..."));
		ProcessMatchCheck();
		break;
		
	case EMatch3State::RevertingSwap:
		// 失败回退动画完成 -> 回到空闲，接受下一次操作
		UE_LOG(LogTemp, Log, TEXT("  -> RevertingSwap finished, back to Idle"));
		GameState = EMatch3State::Idle;
		break;

	case EMatch3State::Clearing:
		{
			// 消除动画完成 -> 执行下落逻辑，填充空格子
			UE_LOG(LogTemp, Log, TEXT("  -> Clearing finished, filling empty tiles..."));
			
			LastFallMoves = FillEmptyTiles();
			GameState = EMatch3State::Falling;
			
			UE_LOG(LogTemp, Log, TEXT("  -> Generated %d fall moves, triggering OnFallAnimTriggered"), LastFallMoves.Num());
			
			// [时机4] 通知UI播放下落动画
			OnFallAnimTriggered(LastFallMoves);
		}
		break;

	case EMatch3State::Falling:
		// 下落动画完成 -> 递归检查新的匹配（连消检测）
		UE_LOG(LogTemp, Log, TEXT("  -> Falling finished, checking matches again (combo check)..."));
		ProcessMatchCheck();
		break;
		
	case EMatch3State::CheckMatching:
	case EMatch3State::Idle:
	default:
		UE_LOG(LogTemp, Warning, TEXT("  -> AdvanceGameState called in unexpected state: %d"), (int32)GameState);
		break;
	}
}

void ADatamanagement::StartSwap(int32 IndexA, int32 IndexB)
{
	GameState = EMatch3State::Swapping;
	UE_LOG(LogTemp, Log, TEXT("StartSwap: State -> Swapping"));
	
	// [时机2] 通知UI播放成功的交换动画
	OnSwapAnimTriggered(IndexA, IndexB, true);
}

void ADatamanagement::RevertSwap(int32 IndexA, int32 IndexB)
{
	GameState = EMatch3State::RevertingSwap;
	UE_LOG(LogTemp, Log, TEXT("RevertSwap: State -> RevertingSwap"));
	
	// [时机2] 通知UI播放失败的交换动画（来回晃动后复位）
	OnSwapAnimTriggered(IndexA, IndexB, false);
}

void ADatamanagement::ProcessMatchCheck()
{
	GameState = EMatch3State::CheckMatching;
	UE_LOG(LogTemp, Log, TEXT("ProcessMatchCheck: State -> CheckMatching"));

	TSet<int32> MatchedIndices;

	// 检查横向匹配
	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize - 2; ++Col)
		{
			int32 Idx = Row * GridSize + Col;
			ETileColor Color = OrbGrid[Idx];
			if (Color == ETileColor::Empty) continue;
			
			if (OrbGrid[Idx + 1] == Color && OrbGrid[Idx + 2] == Color)
			{
				MatchedIndices.Add(Idx);
				MatchedIndices.Add(Idx + 1);
				MatchedIndices.Add(Idx + 2);
				
				// 检查是否超过3个（4连、5连）
				int32 NextCol = Col + 3;
				while (NextCol < GridSize && OrbGrid[Row * GridSize + NextCol] == Color)
				{
					MatchedIndices.Add(Row * GridSize + NextCol);
					NextCol++;
				}
			}
		}
	}

	// 检查纵向匹配
	for (int32 Col = 0; Col < GridSize; ++Col)
	{
		for (int32 Row = 0; Row < GridSize - 2; ++Row)
		{
			int32 Idx = Row * GridSize + Col;
			ETileColor Color = OrbGrid[Idx];
			if (Color == ETileColor::Empty) continue;

			if (OrbGrid[Idx + GridSize] == Color && OrbGrid[Idx + GridSize * 2] == Color)
			{
				MatchedIndices.Add(Idx);
				MatchedIndices.Add(Idx + GridSize);
				MatchedIndices.Add(Idx + GridSize * 2);

				// 检查是否超过3个
				int32 NextRow = Row + 3;
				while (NextRow < GridSize && OrbGrid[NextRow * GridSize + Col] == Color)
				{
					MatchedIndices.Add(NextRow * GridSize + Col);
					NextRow++;
				}
			}
		}
	}

	if (MatchedIndices.Num() > 0)
	{
		TArray<int32> ClearedArray = MatchedIndices.Array();
		GameState = EMatch3State::Clearing;
		
		UE_LOG(LogTemp, Log, TEXT("  -> Found %d matches! State -> Clearing"), ClearedArray.Num());
		
		// 收集触发的特殊效果
		TArray<FSpecialEffectData> TriggeredEffects = CollectSpecialEffects(ClearedArray);
		
		// 清空匹配的方块
		for (int32 Idx : ClearedArray)
		{
			OrbGrid[Idx] = ETileColor::Empty;
		}

		UE_LOG(LogTemp, Log, TEXT("  -> Triggering OnMatchesCleared with %d special effects"), TriggeredEffects.Num());
		
		// [时机3] 通知UI播放消除动画
		// UI可以根据 TriggeredEffects 判断是否有特殊效果需要表现
		// 如果 TriggeredEffects 为空，说明没有触发任何特殊效果
		OnMatchesCleared(ClearedArray, TriggeredEffects);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("  -> No matches found, checking for deadlock..."));
		
		// 检查是否死锁
		if (!HasAnyValidMove())
		{
			UE_LOG(LogTemp, Warning, TEXT("  -> DEADLOCK detected! Reshuffling board..."));
			
			// 重新生成棋盘（保持特殊格子位置不变）
			GenerateBoard();
			
			UE_LOG(LogTemp, Log, TEXT("  -> Triggering OnBoardReshuffle"));
			
			// [时机5] 通知UI播放洗牌动画
			OnBoardReshuffle();
		}
		
		UE_LOG(LogTemp, Log, TEXT("  -> State -> Idle"));
		GameState = EMatch3State::Idle;
	}
}

// ========================================
// 公共接口
// ========================================

void ADatamanagement::InitializeGame()
{
	SelectedTileIndex = -1;
	GameState = EMatch3State::Idle;
	
	int32 TotalTiles = GridSize * GridSize;
	OrbGrid.Init(ETileColor::Empty, TotalTiles);
	
	// 如果特殊格子配置为空，初始化默认配置
	if (SpecialAreaGrid.Num() != TotalTiles)
	{
		SpecialAreaGrid.Init(ESlotEffectType::None, TotalTiles);
		
		// 默认配置：对称分布的3个特殊格子
		// 中心位置 (3, 3) = 索引24 -> 士气提升
		SpecialAreaGrid[24] = ESlotEffectType::MoraleBoost;
		
		// 左侧 (3, 1) = 索引22 -> 加速自己
		SpecialAreaGrid[22] = ESlotEffectType::SpeedUpSelf;
		
		// 右侧 (3, 5) = 索引26 -> 减速敌人
		SpecialAreaGrid[26] = ESlotEffectType::SlowDownEnemy;
		
		UE_LOG(LogTemp, Log, TEXT("InitializeGame: SpecialAreaGrid initialized with default symmetric layout"));
		UE_LOG(LogTemp, Log, TEXT("  -> Center (3,3) = MoraleBoost"));
		UE_LOG(LogTemp, Log, TEXT("  -> Left (3,1) = SpeedUpSelf"));
		UE_LOG(LogTemp, Log, TEXT("  -> Right (3,5) = SlowDownEnemy"));
	}

	// 生成初始棋盘
	GenerateBoard();
	
	// [时机1] 通知UI创建所有方块和特殊格子标识
	// UI应该：
	// 1. 遍历 OrbGrid 创建对应颜色的方块Widget
	// 2. 遍历 SpecialAreaGrid，如果不是None，则在对应位置显示特殊格子标识（光晕/图标等）
	UE_LOG(LogTemp, Log, TEXT("InitializeGame: Board initialized, triggering OnBoardInitialized"));
	OnBoardInitialized();
}

void ADatamanagement::GenerateBoard()
{
	bool bIsValidBoard = false;
	const int32 MaxRetries = 100;
	int32 RetryCount = 0;

	while (!bIsValidBoard && RetryCount < MaxRetries)
	{
		// 1. 随机生成（不覆盖特殊格子配置）
		for (int32 i = 0; i < OrbGrid.Num(); i++)
		{
			int32 ColorInt = FMath::RandRange(0, 3);
			OrbGrid[i] = static_cast<ETileColor>(ColorInt);
		}

		// 2. 检查是否有初始匹配（不允许）
		if (HasMatch())
		{
			RetryCount++;
			continue;
		}

		// 3. 检查是否有解
		if (!HasAnyValidMove())
		{
			RetryCount++;
			continue;
		}
		
		bIsValidBoard = true;
	}
}

bool ADatamanagement::HasAnyValidMove()
{
	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize; ++Col)
		{
			int32 CurrentIdx = Row * GridSize + Col;
			
			// 尝试右交换
			if (Col < GridSize - 1)
			{
				int32 RightIdx = CurrentIdx + 1;
				OrbGrid.Swap(CurrentIdx, RightIdx);
				bool bCanMatch = HasMatch();
				OrbGrid.Swap(CurrentIdx, RightIdx);
				if (bCanMatch) return true;
			}
			
			// 尝试下交换
			if (Row < GridSize - 1)
			{
				int32 DownIdx = CurrentIdx + GridSize;
				OrbGrid.Swap(CurrentIdx, DownIdx);
				bool bCanMatch = HasMatch();
				OrbGrid.Swap(CurrentIdx, DownIdx);
				if (bCanMatch) return true;
			}
		}
	}
	return false;
}

bool ADatamanagement::HandleTileInput(int32 TileIndex)
{
	if (!IsValidIndex(TileIndex))
	{
		return false;
	}

	if (SelectedTileIndex == -1)
	{
		SelectedTileIndex = TileIndex;
		return true;
	}

	if (SelectedTileIndex == TileIndex)
	{
		SelectedTileIndex = -1;
		return true;
	}

	if (IsAdjacent(SelectedTileIndex, TileIndex))
	{
		TrySwap(SelectedTileIndex, TileIndex);
		SelectedTileIndex = -1; 
		return true;
	}

	SelectedTileIndex = TileIndex;
	return true;
}

ETileColor ADatamanagement::GetColorAt(int32 TileIndex) const
{
	if (IsValidIndex(TileIndex))
	{
		return OrbGrid[TileIndex];
	}
	return ETileColor::Empty;
}

ESlotEffectType ADatamanagement::GetSpecialTileType(int32 Index) const
{
	if (SpecialAreaGrid.IsValidIndex(Index))
	{
		return SpecialAreaGrid[Index];
	}
	return ESlotEffectType::None;
}

// ========================================
// 辅助函数
// ========================================

void ADatamanagement::IndexToRowCol(int32 Index, int32& OutRow, int32& OutCol) const
{
	OutRow = Index / GridSize;
	OutCol = Index % GridSize;
}

int32 ADatamanagement::RowColToIndex(int32 Row, int32 Col) const
{
	return Row * GridSize + Col;
}

bool ADatamanagement::HasMatch()
{
	// 横向检查
	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize - 2; ++Col)
		{
			int32 Idx = Row * GridSize + Col;
			ETileColor Color = OrbGrid[Idx];
			if (Color == ETileColor::Empty) continue;
			if (OrbGrid[Idx + 1] == Color && OrbGrid[Idx + 2] == Color)
			{
				return true;
			}
		}
	}

	// 纵向检查
	for (int32 Col = 0; Col < GridSize; ++Col)
	{
		for (int32 Row = 0; Row < GridSize - 2; ++Row)
		{
			int32 Idx = Row * GridSize + Col;
			ETileColor Color = OrbGrid[Idx];
			if (Color == ETileColor::Empty) continue;
			if (OrbGrid[Idx + GridSize] == Color && OrbGrid[Idx + GridSize * 2] == Color)
			{
				return true;
			}
		}
	}

	return false;
}

TArray<FFallMove> ADatamanagement::FillEmptyTiles()
{
	TArray<FFallMove> FallMoves;

	for (int32 Col = 0; Col < GridSize; ++Col)
	{
		TArray<ETileColor> ExistingTiles;
		TArray<int32> ExistingIndices;
		
		// 收集该列现有的非空方块
		for (int32 Row = 0; Row < GridSize; ++Row)
		{
			int32 Idx = Row * GridSize + Col;
			if (OrbGrid[Idx] != ETileColor::Empty)
			{
				ExistingTiles.Add(OrbGrid[Idx]);
				ExistingIndices.Add(Idx);
			}
		}

		int32 MissingCount = GridSize - ExistingTiles.Num();

		// 填充该列
		for (int32 Row = 0; Row < GridSize; ++Row)
		{
			int32 ToIdx = Row * GridSize + Col;
			
			if (Row < MissingCount)
			{
				// 新生成的方块
				int32 ColorInt = FMath::RandRange(0, 3);
				ETileColor NewColor = static_cast<ETileColor>(ColorInt);
				OrbGrid[ToIdx] = NewColor;

				int32 VirtualFromIdx = -(MissingCount - Row);
				FallMoves.Add(FFallMove(VirtualFromIdx, ToIdx, NewColor, true));
			}
			else
			{
				// 下落的方块
				int32 SourceArrayIdx = Row - MissingCount;
				ETileColor ExistingColor = ExistingTiles[SourceArrayIdx];
				int32 FromIdx = ExistingIndices[SourceArrayIdx];
				
				OrbGrid[ToIdx] = ExistingColor;

				if (FromIdx != ToIdx)
				{
					FallMoves.Add(FFallMove(FromIdx, ToIdx, ExistingColor, false));
				}
			}
		}
	}

	return FallMoves;
}

TArray<FSpecialEffectData> ADatamanagement::CollectSpecialEffects(const TArray<int32>& ClearedIndices)
{
	// 按效果类型分组收集索引
	TMap<ESlotEffectType, TArray<int32>> EffectIndicesMap;
	
	// 统计消除的特殊格子
	for (int32 Idx : ClearedIndices)
	{
		if (SpecialAreaGrid.IsValidIndex(Idx))
		{
			ESlotEffectType EffectType = SpecialAreaGrid[Idx];
			if (EffectType != ESlotEffectType::None)
			{
				TArray<int32>& Indices = EffectIndicesMap.FindOrAdd(EffectType);
				Indices.Add(Idx);
				
				UE_LOG(LogTemp, Log, TEXT("  -> Special tile at index %d, type %d"), Idx, (int32)EffectType);
			}
		}
	}
	
	// 构建特殊效果数组
	TArray<FSpecialEffectData> TriggeredEffects;
	for (const auto& Pair : EffectIndicesMap)
	{
		TriggeredEffects.Add(FSpecialEffectData(Pair.Key, Pair.Value));
		UE_LOG(LogTemp, Log, TEXT("  -> Effect: %d, Triggered at %d positions"), (int32)Pair.Key, Pair.Value.Num());
	}
	
	return TriggeredEffects;
}

bool ADatamanagement::IsAdjacent(int32 IndexA, int32 IndexB) const
{
	int32 RowA = IndexA / GridSize;
	int32 ColA = IndexA % GridSize;
	int32 RowB = IndexB / GridSize;
	int32 ColB = IndexB % GridSize;

	return (FMath::Abs(RowA - RowB) + FMath::Abs(ColA - ColB)) == 1;
}

bool ADatamanagement::IsValidIndex(int32 Index) const
{
	return Index >= 0 && Index < OrbGrid.Num();
}

