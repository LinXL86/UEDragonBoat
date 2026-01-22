// Fill out your copyright notice in the Description page of Project Settings.

#include "Datamanagement.h"

ADatamanagement::ADatamanagement()
{
	PrimaryActorTick.bCanEverTick = true;
	SelectedTileIndex = -1;
	PendingSwapIndexA = -1;
	PendingSwapIndexB = -1;
	GameState = EMatch3State::Idle;

	// 士气值系统初始化
	CurrentMorale = 0;
	MaxMorale = 100;
	MoralePerTile = 5;
	SpecialMoraleBonus = 20;
	SkillPoints = 0;
	MaxSkillPoints = 3;

	// 龙舟竞速效果初始化
	SpeedBoostPerTrigger = 50.0f;
	SlowDownPerTrigger = 30.0f;

	// 技能系统初始化（默认装备2个技能）
	EquippedSkills.SetNum(2);
	EquippedSkills[0] = ESkillType::EastWind; // 槽位1：巧借东风
	EquippedSkills[1] = ESkillType::FloodSeven;   // 槽位2：水淹七军

	// 初始化技能配置
	FSkillConfig EastWindConfig;
	EastWindConfig.Duration = 5.0f;
	EastWindConfig.EffectValue = 250.0f;  // 速度加成
	SkillConfigs.Add(ESkillType::EastWind, EastWindConfig);

	FSkillConfig FloodConfig;
	FloodConfig.Duration = 3.0f;
	FloodConfig.EffectValue = 0.0f;
	SkillConfigs.Add(ESkillType::FloodSeven, FloodConfig);

	FSkillConfig FogConfig;
	FogConfig.Duration = 5.0f;
	FogConfig.EffectValue = 0.5f;  // 雾的透明度
	SkillConfigs.Add(ESkillType::HeavyFog, FogConfig);

	FSkillConfig ChainConfig;
	ChainConfig.Duration = 5.0f;
	ChainConfig.EffectValue = 4.0f;  // 锁定格子数量
	SkillConfigs.Add(ESkillType::IronChain, ChainConfig);

	// 空城计配置
	FSkillConfig EmptyCityConfig;
	EmptyCityConfig.Duration = 8.0f;   // 免疫持续时间
	EmptyCityConfig.EffectValue = 0.0f;
	SkillConfigs.Add(ESkillType::EmptyCity, EmptyCityConfig);

	// AI技能系统初始化
	bEnableAISkills = true;  // 默认启用
	AISkillIntervalMin = 10.0f;  // 最小10秒
	AISkillIntervalMax = 20.0f;  // 最大20秒
	bRandomizeAISkillsEachRace = false;  // 默认不随机，使用固定配置

	// AI1 配置2个技能：
	AI1_EquippedSkills.SetNum(2);
	AI1_EquippedSkills[0] = ESkillType::EastWind;     // 槽位1：巧借东风
	AI1_EquippedSkills[1] = ESkillType::FloodSeven;   // 槽位2：水淹七军

	// AI2 配置2个技能：
	AI2_EquippedSkills.SetNum(2);
	AI2_EquippedSkills[0] = ESkillType::HeavyFog;     // 槽位1：大雾
	AI2_EquippedSkills[1] = ESkillType::IronChain;    // 槽位2：铁索连环

	// 初始化技能目标类型映射
	SkillTargetTypeMap.Add(ESkillType::EastWind, ESkillTargetType::Self);      // 增益：给自己加速
	SkillTargetTypeMap.Add(ESkillType::FloodSeven, ESkillTargetType::Enemy); // 减益：攻击敌人
	SkillTargetTypeMap.Add(ESkillType::HeavyFog, ESkillTargetType::Enemy);     // 减益：攻击敌人
	SkillTargetTypeMap.Add(ESkillType::IronChain, ESkillTargetType::Enemy);    // 减益：攻击敌人
	SkillTargetTypeMap.Add(ESkillType::EmptyCity, ESkillTargetType::Self);     // 增益：保护自己
}

void ADatamanagement::BeginPlay()
{
	Super::BeginPlay();
	InitializeGame();

	// AI 技能系统启动由 GameMode 控制，在合适的时机调用 StartAISkillSystem()
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

	// 横向匹配
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
				
				// 检查是否超过3连（4连、5连）
				int32 NextCol = Col + 3;
				while (NextCol < GridSize && OrbGrid[Row * GridSize + NextCol] == Color)
				{
					MatchedIndices.Add(Row * GridSize + NextCol);
					NextCol++;
				}
			}
		}
	}

	// 纵向匹配
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

				// 检查是否超过3连
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
		
		UE_LOG(LogTemp, Log, TEXT("-> Found %d matches! State -> Clearing"), ClearedArray.Num());
		
		// 收集触发的特殊效果
		TArray<FSpecialEffectData> TriggeredEffects = CollectSpecialEffects(ClearedArray);
		
		// 计算并添加士气值
		int32 MoraleReward = CalculateMoraleReward(ClearedArray.Num(), TriggeredEffects);
		if (MoraleReward > 0)
		{
			AddMorale(MoraleReward);
		}

		// 触发龙舟竞速效果
		TriggerRaceEffects(TriggeredEffects);

		// 清空匹配的方块
		for (int32 Idx : ClearedArray)
		{
			OrbGrid[Idx] = ETileColor::Empty;
		}

		UE_LOG(LogTemp, Log, TEXT("  -> Triggering OnMatchesCleared with %d special effects"), TriggeredEffects.Num());
		
		// [时机3] 通知UI播放消除动画
		OnMatchesCleared(ClearedArray, TriggeredEffects);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("  -> No matches found, checking for deadlock..."));
		
		// 检查是否死锁
		if (!HasAnyValidMove())
		{
			UE_LOG(LogTemp, Warning, TEXT("  -> DEADLOCK detected! Reshuffling board..."));
			
			// 重新生成棋盘（特殊格子位置不变）
			GenerateBoard();
			
			UE_LOG(LogTemp, Log, TEXT("  -> Triggering OnBoardReshuffle"));
			
			// [时机5] 通知UI棋盘洗牌动画
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

	// 重置士气值系统
	CurrentMorale = 0;
	SkillPoints = 0;
	
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
		// 1. 随机生成棋盘
		for (int32 i = 0; i < OrbGrid.Num(); i++)
		{
			int32 ColorInt = FMath::RandRange(0, 3);
			OrbGrid[i] = static_cast<ETileColor>(ColorInt);
		}

		// 2. 消除所有初始匹配（逐个方块检查并替换）
		bool bHadMatches = true;
		int32 FixAttempts = 0;
		const int32 MaxFixAttempts = 200;
		
		while (bHadMatches && FixAttempts < MaxFixAttempts)
		{
			bHadMatches = false;
			FixAttempts++;
			
			// 遍历每个格子，检查是否形成匹配
			for (int32 Row = 0; Row < GridSize; ++Row)
			{
				for (int32 Col = 0; Col < GridSize; ++Col)
				{
					int32 Idx = Row * GridSize + Col;
					ETileColor CurrentColor = OrbGrid[Idx];
					
					if (CurrentColor == ETileColor::Empty)
						continue;
					
					bool bIsInMatch = false;
					
					// 检查横向匹配（当前格子是否是横向3连的一部分）
					if (Col >= 2)
					{
						// 检查左侧两个
						if (OrbGrid[Idx - 1] == CurrentColor && OrbGrid[Idx - 2] == CurrentColor)
						{
							bIsInMatch = true;
						}
					}
					if (Col >= 1 && Col < GridSize - 1)
					{
						// 检查左右各一个
						if (OrbGrid[Idx - 1] == CurrentColor && OrbGrid[Idx + 1] == CurrentColor)
						{
							bIsInMatch = true;
						}
					}
					if (Col < GridSize - 2)
					{
						// 检查右侧两个
						if (OrbGrid[Idx + 1] == CurrentColor && OrbGrid[Idx + 2] == CurrentColor)
						{
							bIsInMatch = true;
						}
					}
					
					// 检查纵向匹配
					if (Row >= 2)
					{
						// 检查上方两个
						if (OrbGrid[Idx - GridSize] == CurrentColor && OrbGrid[Idx - GridSize * 2] == CurrentColor)
						{
							bIsInMatch = true;
						}
					}
					if (Row >= 1 && Row < GridSize - 1)
					{
						// 检查上下各一个
						if (OrbGrid[Idx - GridSize] == CurrentColor && OrbGrid[Idx + GridSize] == CurrentColor)
						{
							bIsInMatch = true;
						}
					}
					if (Row < GridSize - 2)
					{
						// 检查下方两个
						if (OrbGrid[Idx + GridSize] == CurrentColor && OrbGrid[Idx + GridSize * 2] == CurrentColor)
						{
							bIsInMatch = true;
						}
					}
					
					// 如果发现匹配，替换为不会形成匹配的颜色
					if (bIsInMatch)
					{
						bHadMatches = true;
						
						// 尝试所有可能的颜色，找一个不会形成匹配的
						TArray<ETileColor> AvailableColors = {
							ETileColor::Red,
							ETileColor::Blue,
							ETileColor::Green,
							ETileColor::Yellow
						};
						
						// 移除当前颜色
						AvailableColors.Remove(CurrentColor);
						
						// 移除左侧相邻的颜色（如果连续2个）
						if (Col >= 2 && OrbGrid[Idx - 1] == OrbGrid[Idx - 2])
						{
							AvailableColors.Remove(OrbGrid[Idx - 1]);
						}
						
						// 移除上方相邻的颜色（如果连续2个）
						if (Row >= 2 && OrbGrid[Idx - GridSize] == OrbGrid[Idx - GridSize * 2])
						{
							AvailableColors.Remove(OrbGrid[Idx - GridSize]);
						}
						
						// 随机选择一个安全的颜色
						if (AvailableColors.Num() > 0)
						{
							int32 RandomIndex = FMath::RandRange(0, AvailableColors.Num() - 1);
							OrbGrid[Idx] = AvailableColors[RandomIndex];
						}
						else
						{
							// 极端情况：所有颜色都会形成匹配，随机选一个
							int32 ColorInt = FMath::RandRange(0, 3);
							OrbGrid[Idx] = static_cast<ETileColor>(ColorInt);
						}
					}
				}
			}
		}
		
		if (FixAttempts >= MaxFixAttempts)
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateBoard: Failed to fix initial matches after %d attempts, retrying..."), MaxFixAttempts);
			RetryCount++;
			continue;
		}

		// 3. 最终验证：确保没有匹配
		if (HasMatch())
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateBoard: Board still has matches after fixing, retrying..."));
			RetryCount++;
			continue;
		}

		// 4. 检查是否有可用移动
		if (!HasAnyValidMove())
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateBoard: Board has no valid moves, retrying..."));
			RetryCount++;
			continue;
		}
		
		bIsValidBoard = true;
		UE_LOG(LogTemp, Log, TEXT("GenerateBoard: Successfully generated valid board (Retries: %d, FixAttempts: %d)"), RetryCount, FixAttempts);
	}
	
	if (RetryCount >= MaxRetries)
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateBoard: Failed to generate valid board after %d retries!"), MaxRetries);
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

// ========================================
// 士气值系统
// ========================================

void ADatamanagement::AddMorale(int32 Amount)
{
	if (Amount <= 0)
		return;

	// 如果技能点已满，拒绝添加士气值
	if (SkillPoints >= MaxSkillPoints)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddMorale: Skill Points are full (%d/%d), cannot add morale!"), SkillPoints, MaxSkillPoints);
		
		// 确保士气值为0
		if (CurrentMorale != 0)
		{
			CurrentMorale = 0;
			OnMoraleChanged(CurrentMorale, MaxMorale, 0);
		}
		
		return;
	}

	int32 OldMorale = CurrentMorale;
	CurrentMorale += Amount;

	UE_LOG(LogTemp, Log, TEXT("AddMorale: +%d (Total: %d/%d)"), Amount, CurrentMorale, MaxMorale);

	// 通知UI士气值变化
	OnMoraleChanged(CurrentMorale, MaxMorale, Amount);

	// 检查是否需要转换为技能点
	CheckMoraleToSkillPoint();
}

float ADatamanagement::GetMoraleProgress() const
{
	if (MaxMorale <= 0)
		return 0.0f;

	return FMath::Clamp((float)CurrentMorale / (float)MaxMorale, 0.0f, 1.0f);
}

bool ADatamanagement::ConsumeSkillPoint(int32 Amount)
{
	if (SkillPoints < Amount)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConsumeSkillPoint: Not enough skill points! (Have: %d, Need: %d)"), SkillPoints, Amount);
		return false;
	}

	SkillPoints -= Amount;
	UE_LOG(LogTemp, Log, TEXT("ConsumeSkillPoint: -%d (Remaining: %d)"), Amount, SkillPoints);

	// 通知UI技能点变化
	OnSkillPointChanged(SkillPoints, MaxSkillPoints);

	return true;
}

int32 ADatamanagement::CalculateMoraleReward(int32 TileCount, const TArray<FSpecialEffectData>& TriggeredEffects)
{
	// 基础士气值：每个方块贡献固定值
	int32 TotalMorale = TileCount * MoralePerTile;

	// 特殊格子额外士气值
	for (const FSpecialEffectData& Effect : TriggeredEffects)
	{
		if (Effect.EffectType == ESlotEffectType::MoraleBoost)
		{
			int32 BonusMorale = Effect.TriggerIndices.Num() * SpecialMoraleBonus;
			TotalMorale += BonusMorale;
			UE_LOG(LogTemp, Log, TEXT("  -> MoraleBoost triggered %d times, bonus: +%d"), 
				Effect.TriggerIndices.Num(), BonusMorale);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("  -> Total Morale Reward: %d (Base: %d tiles x %d)"), 
		TotalMorale, TileCount, MoralePerTile);

	return TotalMorale;
}

void ADatamanagement::CheckMoraleToSkillPoint()
{
	// 士气值满时转换为技能点
	while (CurrentMorale >= MaxMorale && SkillPoints < MaxSkillPoints)
	{
		CurrentMorale -= MaxMorale;
		SkillPoints++;

		UE_LOG(LogTemp, Log, TEXT("CheckMoraleToSkillPoint: Morale Full! Converted to Skill Point (Total: %d/%d)"), 
			SkillPoints, MaxSkillPoints);

		// 通知UI技能点变化
		OnSkillPointChanged(SkillPoints, MaxSkillPoints);

		// 再次通知UI士气值变化（显示重置后的值）
// 		OnMoraleChanged(CurrentMorale, MaxMorale, 0);
		
		// 如果技能点已满，强制清零士气值
		if (SkillPoints >= MaxSkillPoints)
		{
			CurrentMorale = 0;
			UE_LOG(LogTemp, Warning, TEXT("CheckMoraleToSkillPoint: Skill Points full! Morale reset to 0"));
			OnMoraleChanged(CurrentMorale, MaxMorale, 0);
		}
	}

	// 确保士气值不超过上限（额外的安全检查）
	if (CurrentMorale > MaxMorale)
	{
		UE_LOG(LogTemp, Warning, TEXT("CheckMoraleToSkillPoint: Morale exceeded max! Capping at %d"), MaxMorale);
		CurrentMorale = MaxMorale;
		OnMoraleChanged(CurrentMorale, MaxMorale, 0);
	}
}

// ========================================
// 测试/调试函数
// ========================================

void ADatamanagement::Debug_SetMorale(int32 NewMorale)
{
	CurrentMorale = FMath::Clamp(NewMorale, 0, MaxMorale * MaxSkillPoints);
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] SetMorale: %d"), CurrentMorale);
	OnMoraleChanged(CurrentMorale, MaxMorale, 0);
}

void ADatamanagement::Debug_SetSkillPoints(int32 NewSkillPoints)
{
	SkillPoints = FMath::Clamp(NewSkillPoints, 0, MaxSkillPoints);
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] SetSkillPoints: %d"), SkillPoints);
	OnSkillPointChanged(SkillPoints, MaxSkillPoints);
}

void ADatamanagement::Debug_SimulateMatch(int32 TileCount, bool bIncludeSpecialBonus)
{
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] SimulateMatch: %d tiles, SpecialBonus: %s"), 
		TileCount, bIncludeSpecialBonus ? TEXT("Yes") : TEXT("No"));

	// 计算士气值
	int32 MoraleReward = TileCount * MoralePerTile;
	if (bIncludeSpecialBonus)
	{
		MoraleReward += SpecialMoraleBonus;
	}

	UE_LOG(LogTemp, Log, TEXT("  -> Morale Reward: %d"), MoraleReward);

	// 添加士气值
	AddMorale(MoraleReward);
}

// ========================================
// 龙舟竞速效果系统
// ========================================

void ADatamanagement::TriggerRaceEffects(const TArray<FSpecialEffectData>& TriggeredEffects)
{
	for (const FSpecialEffectData& Effect : TriggeredEffects)
	{
		int32 TriggerCount = Effect.TriggerIndices.Num();

		if (Effect.EffectType == ESlotEffectType::SpeedUpSelf)
		{
			UE_LOG(LogTemp, Log, TEXT("  -> Player SpeedUp triggered %d times"), TriggerCount);
			OnPlayerSpeedUpTriggered(TriggerCount, SpeedBoostPerTrigger);
		}
		else if (Effect.EffectType == ESlotEffectType::SlowDownEnemy)
		{
			UE_LOG(LogTemp, Log, TEXT("  -> Player SlowDown Enemy triggered %d times"), TriggerCount);
			OnPlayerSlowDownEnemyTriggered(TriggerCount, SlowDownPerTrigger);
		}
		// MoraleBoost已经在CalculateMoraleReward中处理，这里不需要额外操作
	}
}

// ========================================
// 技能系统
// ========================================

bool ADatamanagement::TryCastSkill(int32 SlotIndex)
{
	// 检查槽位有效性
	if (!EquippedSkills.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("TryCastSkill: Invalid slot index %d"), SlotIndex);
		return false;
	}

	// 检查技能点
	if (!IsSkillAvailable(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("TryCastSkill: Not enough skill points!"));
		return false;
	}

	ESkillType SkillType = EquippedSkills[SlotIndex];

	// 获取技能配置
	FSkillConfig* Config = SkillConfigs.Find(SkillType);
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("TryCastSkill: Skill config not found for skill type %d!"), (int32)SkillType);
		return false;
	}

	// 消耗1个技能点
	if (!ConsumeSkillPoint(1))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("TryCastSkill: Success! Slot=%d, Type=%d, Duration=%.2f, EffectValue=%.2f"), 
		SlotIndex, (int32)SkillType, Config->Duration, Config->EffectValue);

	// 触发蓝图事件
	OnSkillCasted(SkillType, *Config);

	return true;
}

bool ADatamanagement::IsSkillAvailable(int32 SlotIndex) const
{
	if (!EquippedSkills.IsValidIndex(SlotIndex))
		return false;

	return SkillPoints >= 1;
}

ESkillType ADatamanagement::GetEquippedSkill(int32 SlotIndex) const
{
	if (EquippedSkills.IsValidIndex(SlotIndex))
		return EquippedSkills[SlotIndex];

	// 默认返回巧借东风
	return ESkillType::EastWind;
}

void ADatamanagement::SetEquippedSkill(int32 SlotIndex, ESkillType NewSkill)
{
	if (EquippedSkills.IsValidIndex(SlotIndex))
	{
		EquippedSkills[SlotIndex] = NewSkill;
		
	}
	else
	{
		
	}
}

// ========================================
// AI技能系统
// ========================================

void ADatamanagement::StartAISkillSystem()
{
	if (!bEnableAISkills)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartAISkillSystem: AI skills are disabled!"));
		return;
	}

	// 如果启用了每局随机技能，重新配置AI技能
	if (bRandomizeAISkillsEachRace)
	{
		RandomizeAISkills();
	}

	UE_LOG(LogTemp, Log, TEXT("StartAISkillSystem: Starting AI skill system..."));
	UE_LOG(LogTemp, Log, TEXT("  -> AI1: Slot0=%d, Slot1=%d"), (int32)AI1_EquippedSkills[0], (int32)AI1_EquippedSkills[1]);
	UE_LOG(LogTemp, Log, TEXT("  -> AI2: Slot0=%d, Slot1=%d"), (int32)AI2_EquippedSkills[0], (int32)AI2_EquippedSkills[1]);
	
	// 开始第一次 AI 技能释放
	ScheduleNextAISkill();
}

void ADatamanagement::TriggerAISkill()
{
	// 随机选择一个施法AI（AI1 或 AI2）
	EAIBoatIndex CasterAI = (FMath::RandBool()) ? EAIBoatIndex::AI1 : EAIBoatIndex::AI2;

	// 获取该 AI 的装备技能列表
	const TArray<ESkillType>& AvailableSkills = 
		(CasterAI == EAIBoatIndex::AI1) ? AI1_EquippedSkills : AI2_EquippedSkills;

	// 如果没有可用技能，直接返回
	if (AvailableSkills.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TriggerAISkill: %s has no equipped skills!"),
			(CasterAI == EAIBoatIndex::AI1) ? TEXT("AI1") : TEXT("AI2"));
		ScheduleNextAISkill();
		return;
	}

	// 随机选择一个技能槽
	int32 SlotIndex = FMath::RandRange(0, AvailableSkills.Num() - 1);
	ESkillType SelectedSkill = AvailableSkills[SlotIndex];

	// 获取技能配置
	FSkillConfig* Config = SkillConfigs.Find(SelectedSkill);
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("TriggerAISkill: Skill config not found for skill type %d!"), (int32)SelectedSkill);
		ScheduleNextAISkill();
		return;
	}

	// 获取技能目标类型
	ESkillTargetType TargetType = GetSkillTargetType(SelectedSkill);

	// 确定目标
	EAIBoatIndex TargetAI = EAIBoatIndex::AI1;  // 默认值
	bool bTargetIsPlayer = false;

	if (TargetType == ESkillTargetType::Self)
	{
		// 增益技能：施加给自己
		TargetAI = CasterAI;
		bTargetIsPlayer = false;

		UE_LOG(LogTemp, Log, TEXT("TriggerAISkill: %s casts Slot %d [%d] (BUFF) on SELF! Duration=%.2f"),
			(CasterAI == EAIBoatIndex::AI1) ? TEXT("AI1") : TEXT("AI2"),
			SlotIndex, (int32)SelectedSkill, Config->Duration);
	}
	else
	{
		// 减益技能：随机选择敌人（玩家或另一个AI）
		// 50% 概率攻击玩家，50% 概率攻击另一个AI
		bTargetIsPlayer = FMath::RandBool();

		if (bTargetIsPlayer)
		{
			UE_LOG(LogTemp, Log, TEXT("TriggerAISkill: %s casts Slot %d [%d] (DEBUFF) on PLAYER! Duration=%.2f"),
				(CasterAI == EAIBoatIndex::AI1) ? TEXT("AI1") : TEXT("AI2"),
				SlotIndex, (int32)SelectedSkill, Config->Duration);
		}
		else
		{
			// 攻击另一个AI
			TargetAI = (CasterAI == EAIBoatIndex::AI1) ? EAIBoatIndex::AI2 : EAIBoatIndex::AI1;

			UE_LOG(LogTemp, Log, TEXT("TriggerAISkill: %s casts Slot %d [%d] (DEBUFF) on %s! Duration=%.2f"),
				(CasterAI == EAIBoatIndex::AI1) ? TEXT("AI1") : TEXT("AI2"),
				SlotIndex, (int32)SelectedSkill,
				(TargetAI == EAIBoatIndex::AI1) ? TEXT("AI1") : TEXT("AI2"),
				Config->Duration);
		}
	}

	// 触发蓝图事件
	OnAISkillCasted(CasterAI, SelectedSkill, TargetType, TargetAI, bTargetIsPlayer, *Config);

	// 调度下一次释放
	ScheduleNextAISkill();
}

void ADatamanagement::ScheduleNextAISkill()
{
	if (!bEnableAISkills)
		return;

	// 随机生成下一次释放的时间间隔
	float RandomInterval = FMath::RandRange(AISkillIntervalMin, AISkillIntervalMax);

	UE_LOG(LogTemp, Log, TEXT("ScheduleNextAISkill: Next AI skill in %.2f seconds"), RandomInterval);

	// 设置Timer
	GetWorld()->GetTimerManager().SetTimer(
		AISkillTimerHandle,
		this,
		&ADatamanagement::TriggerAISkill,
		RandomInterval,
		false  // 不重复，每次手动调度
	);
}

ESkillTargetType ADatamanagement::GetSkillTargetType(ESkillType SkillType) const
{
	// 从映射表中查找技能目标类型
	const ESkillTargetType* TargetTypePtr = SkillTargetTypeMap.Find(SkillType);
	if (TargetTypePtr)
	{
		return *TargetTypePtr;
	}

	// 默认返回减益技能（攻击敌人）
	UE_LOG(LogTemp, Warning, TEXT("GetSkillTargetType: Skill %d not found in map, defaulting to Enemy"), (int32)SkillType);
	return ESkillTargetType::Enemy;
}

void ADatamanagement::RandomizeAISkills()
{
	// 所有可用技能类型
	TArray<ESkillType> AllSkills = {
		ESkillType::EastWind,
		ESkillType::FloodSeven,
		ESkillType::HeavyFog,
		ESkillType::IronChain,
		ESkillType::EmptyCity
	};

	// 为 AI1 随机选择 2 个不重复的技能
	TArray<ESkillType> AI1_RandomSkills = AllSkills;
	
	int32 AI1_Slot0_Index = FMath::RandRange(0, AI1_RandomSkills.Num() - 1);
	AI1_EquippedSkills[0] = AI1_RandomSkills[AI1_Slot0_Index];
	AI1_RandomSkills.RemoveAt(AI1_Slot0_Index);  // 移除已选技能，避免重复
	
	int32 AI1_Slot1_Index = FMath::RandRange(0, AI1_RandomSkills.Num() - 1);
	AI1_EquippedSkills[1] = AI1_RandomSkills[AI1_Slot1_Index];

	// 为 AI2 随机选择 2 个不重复的技能
	TArray<ESkillType> AI2_RandomSkills = AllSkills;
	
	int32 AI2_Slot0_Index = FMath::RandRange(0, AI2_RandomSkills.Num() - 1);
	AI2_EquippedSkills[0] = AI2_RandomSkills[AI2_Slot0_Index];
	AI2_RandomSkills.RemoveAt(AI2_Slot0_Index);
	
	int32 AI2_Slot1_Index = FMath::RandRange(0, AI2_RandomSkills.Num() - 1);
	AI2_EquippedSkills[1] = AI2_RandomSkills[AI2_Slot1_Index];

	UE_LOG(LogTemp, Log, TEXT("RandomizeAISkills: AI skills randomized for this race!"));
}

