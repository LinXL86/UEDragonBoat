// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Datamanagement.generated.h"

// 方块颜色
UENUM(BlueprintType)
enum class ETileColor : uint8
{
	Red		UMETA(DisplayName = "Red"),
	Blue	UMETA(DisplayName = "Blue"),
	Green	UMETA(DisplayName = "Green"),
	Yellow	UMETA(DisplayName = "Yellow"),
	Empty	UMETA(DisplayName = "Empty")
};

// 特殊区域效果类型
UENUM(BlueprintType)
enum class ESlotEffectType : uint8
{
	None			UMETA(DisplayName = "None"),
	SpeedUpSelf		UMETA(DisplayName = "Speed Up Self"),
	SlowDownEnemy	UMETA(DisplayName = "Slow Down Enemy"),
	MoraleBoost		UMETA(DisplayName = "Morale Boost")
};

// 游戏流程状态
UENUM(BlueprintType)
enum class EMatch3State : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	Swapping		UMETA(DisplayName = "Swapping (Anim)"),
	CheckMatching	UMETA(DisplayName = "Checking Matches"),
	Clearing		UMETA(DisplayName = "Clearing (Anim)"),
	Falling			UMETA(DisplayName = "Falling (Anim)"),
	RevertingSwap	UMETA(DisplayName = "Reverting Swap (Anim)")
};

// 方块下落移动信息
USTRUCT(BlueprintType)
struct FFallMove
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match3 Fall")
	int32 FromIndex;

	UPROPERTY(BlueprintReadOnly, Category = "Match3 Fall")
	int32 ToIndex;

	UPROPERTY(BlueprintReadOnly, Category = "Match3 Fall")
	ETileColor Color;

	UPROPERTY(BlueprintReadOnly, Category = "Match3 Fall")
	bool bIsNewTile;

	FFallMove()
		: FromIndex(-1), ToIndex(-1), Color(ETileColor::Empty), bIsNewTile(false)
	{}

	FFallMove(int32 InFrom, int32 InTo, ETileColor InColor, bool bNew)
		: FromIndex(InFrom), ToIndex(InTo), Color(InColor), bIsNewTile(bNew)
	{}
};

// 特殊效果数据
USTRUCT(BlueprintType)
struct FSpecialEffectData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match3 Special")
	ESlotEffectType EffectType;

	UPROPERTY(BlueprintReadOnly, Category = "Match3 Special")
	TArray<int32> TriggerIndices;  // 触发该效果的所有格子索引

	FSpecialEffectData()
		: EffectType(ESlotEffectType::None)
	{}

	FSpecialEffectData(ESlotEffectType InType)
		: EffectType(InType)
	{}

	FSpecialEffectData(ESlotEffectType InType, const TArray<int32>& InIndices)
		: EffectType(InType), TriggerIndices(InIndices)
	{}
};

UCLASS()
class DRAGONBOAT_API ADatamanagement : public AActor
{
	GENERATED_BODY()
	
public:	
	ADatamanagement();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// 棋盘大小
	const int32 GridSize = 7;

	// 方块网格数据 (7x7 = 49个方块)
	UPROPERTY(BlueprintReadOnly, Category = "Match3 Data")
	TArray<ETileColor> OrbGrid;

	// 特殊区域网格配置 - 在编辑器中手动配置
	// 索引计算：行号 * 7 + 列号
	// 例如：第2行第3列 = 2*7+3 = 17
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match3 Config")
	TArray<ESlotEffectType> SpecialAreaGrid;

	// 当前选中的方块索引 (-1表示未选中)
	UPROPERTY(BlueprintReadOnly, Category = "Match3 State")
	int32 SelectedTileIndex;

	// 当前游戏状态
	UPROPERTY(BlueprintReadOnly, Category = "Match3 State")
	EMatch3State GameState;

	// 最后一次下落移动记录
	UPROPERTY(BlueprintReadOnly, Category = "Match3 State")
	TArray<FFallMove> LastFallMoves;

	// ========== 公共接口 ==========

	// 初始化游戏（生成初始棋盘）
	UFUNCTION(BlueprintCallable, Category = "Match3 Logic")
	void InitializeGame();

	// 处理方块点击输入（使用索引）
	UFUNCTION(BlueprintCallable, Category = "Match3 Logic")
	bool HandleTileInput(int32 TileIndex);

	// 获取指定位置的方块颜色（使用索引）
	UFUNCTION(BlueprintPure, Category = "Match3 Logic")
	ETileColor GetColorAt(int32 TileIndex) const;

	// 获取指定位置的特殊格子类型
	UFUNCTION(BlueprintPure, Category = "Match3 Logic")
	ESlotEffectType GetSpecialTileType(int32 Index) const;

	// 推进游戏状态 (UI动画完成后调用)
	UFUNCTION(BlueprintCallable, Category = "Match3 Logic")
	void AdvanceGameState();

	// ========== 辅助函数（蓝图可用）==========

	// 将索引转换为行列坐标
	UFUNCTION(BlueprintPure, Category = "Match3 Logic")
	void IndexToRowCol(int32 Index, int32& OutRow, int32& OutCol) const;

	// 将行列坐标转换为索引
	UFUNCTION(BlueprintPure, Category = "Match3 Logic")
	int32 RowColToIndex(int32 Row, int32 Col) const;

	// ========== UI通知事件 ==========

	// [时机1] 棋盘初始化完成 - UI需要创建所有方块和特殊格子标识
	UFUNCTION(BlueprintImplementableEvent, Category = "Match3 Events")
	void OnBoardInitialized();

	// [时机2] 交换动画触发
	UFUNCTION(BlueprintImplementableEvent, Category = "Match3 Events")
	void OnSwapAnimTriggered(int32 IndexA, int32 IndexB, bool bIsSuccessful);

	// [时机3] 消除动画触发
	// ClearedIndices: 被消除的方块索引
	// TriggeredEffects: 被触发的特殊效果（如果没有触发则数组为空）
	UFUNCTION(BlueprintImplementableEvent, Category = "Match3 Events")
	void OnMatchesCleared(const TArray<int32>& ClearedIndices, const TArray<FSpecialEffectData>& TriggeredEffects);

	// [时机4] 下落动画触发
	UFUNCTION(BlueprintImplementableEvent, Category = "Match3 Events")
	void OnFallAnimTriggered(const TArray<FFallMove>& FallMoves);

	// [时机5] 棋盘重新洗牌
	UFUNCTION(BlueprintImplementableEvent, Category = "Match3 Events")
	void OnBoardReshuffle();

private:
	// 尝试交换两个方块
	bool TrySwap(int32 IndexA, int32 IndexB);
	
	// 开始交换
	void StartSwap(int32 IndexA, int32 IndexB);
	
	// 检查匹配
	void ProcessMatchCheck();
	
	// 还原交换
	void RevertSwap(int32 IndexA, int32 IndexB);
	
	// 判断两个索引是否相邻
	bool IsAdjacent(int32 IndexA, int32 IndexB) const;
	
	// 判断索引是否有效
	bool IsValidIndex(int32 Index) const;
	
	// 检查是否有匹配
	bool HasMatch();
	
	// 填充空格子
	TArray<FFallMove> FillEmptyTiles();
	
	// 收集特殊效果
	TArray<FSpecialEffectData> CollectSpecialEffects(const TArray<int32>& ClearedIndices);
	
	// 生成棋盘
	void GenerateBoard();
	
	// 检查是否有可用移动
	bool HasAnyValidMove();
	
	// 待交换的索引
	int32 PendingSwapIndexA;
	int32 PendingSwapIndexB;
};

