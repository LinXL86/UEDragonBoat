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

// 大招技能类型
UENUM(BlueprintType)
enum class ESkillType : uint8
{
	EastWind		UMETA(DisplayName = "East Wind"),		// 巧借东风
	FloodSeven		UMETA(DisplayName = "Flood Seven"),		// 水淹七军
	HeavyFog		UMETA(DisplayName = "Heavy Fog"),		// 大雾
	IronChain		UMETA(DisplayName = "Iron Chain"),		// 铁索连环
	EmptyCity		UMETA(DisplayName = "Empty City")		// 空城计
};

// 技能目标类型（用于AI选择目标）
UENUM(BlueprintType)
enum class ESkillTargetType : uint8
{
	Self		UMETA(DisplayName = "Self"),		// 增益技能（对自己）
	Enemy		UMETA(DisplayName = "Enemy")		// 减益技能（对敌人）
};

// AI龙舟索引（用于区分不同AI）
UENUM(BlueprintType)
enum class EAIBoatIndex : uint8
{
	AI1		UMETA(DisplayName = "AI Boat 1"),		// AI龙舟1
	AI2		UMETA(DisplayName = "AI Boat 2")		// AI龙舟2
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

// 技能配置数据
USTRUCT(BlueprintType)
struct FSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Duration;  // 持续时间（秒）

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float EffectValue;  // 效果数值

	FSkillConfig()
		: Duration(5.0f), EffectValue(100.0f)
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

	// ========== 士气值系统 ==========

	// 当前士气值
	UPROPERTY(BlueprintReadOnly, Category = "Morale System")
	int32 CurrentMorale;

	// 士气值上限（满时转换为技能点）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morale System")
	int32 MaxMorale;

	// 每个消除方块贡献的士气值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morale System")
	int32 MoralePerTile;

	// 特殊格子（士气提升）额外的士气值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morale System")
	int32 SpecialMoraleBonus;

	// 当前技能点（最多3个）
	UPROPERTY(BlueprintReadOnly, Category = "Morale System")
	int32 SkillPoints;

	// 最大技能点数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morale System")
	int32 MaxSkillPoints;

	// ========== 龙舟竞速效果配置 ==========

	// 加速效果数值（每次触发的加速量）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race Effects")
	float SpeedBoostPerTrigger;

	// 减速效果数值（每次触发的减速量）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race Effects")
	float SlowDownPerTrigger;

	// ========== 技能系统 ==========

	// 玩家装备的技能（2个槽位）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill System")
	TArray<ESkillType> EquippedSkills;

	// 技能配置表
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill System")
	TMap<ESkillType, FSkillConfig> SkillConfigs;

	// ========== AI技能系统 ==========

	// AI是否启用自动释放技能
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Skill System")
	bool bEnableAISkills;

	// AI释放技能的最小间隔（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Skill System")
	float AISkillIntervalMin;

	// AI释放技能的最大间隔（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Skill System")
	float AISkillIntervalMax;

	// AI是否每局随机技能（启用后AI技能会在每次比赛开始时重新随机）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Skill System")
	bool bRandomizeAISkillsEachRace;

	// AI1 装备的技能（2个槽位）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Skill System")
	TArray<ESkillType> AI1_EquippedSkills;

	// AI2 装备的技能（2个槽位）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Skill System")
	TArray<ESkillType> AI2_EquippedSkills;

	// AI技能目标类型映射（用于判断技能是增益还是减益）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Skill System")
	TMap<ESkillType, ESkillTargetType> SkillTargetTypeMap;

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

	// ========== 士气值系统接口 ==========

	// 添加士气值
	UFUNCTION(BlueprintCallable, Category = "Morale System")
	void AddMorale(int32 Amount);

	// 获取当前士气值百分比（0.0-1.0）
	UFUNCTION(BlueprintPure, Category = "Morale System")
	float GetMoraleProgress() const;

	// 消耗技能点
	UFUNCTION(BlueprintCallable, Category = "Morale System")
	bool ConsumeSkillPoint(int32 Amount = 1);

	// ========== 技能系统接口 ==========

	// UI调用：释放指定槽位的技能（SlotIndex: 0 或 1）
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	bool TryCastSkill(int32 SlotIndex);

	// 检查指定槽位的技能是否可用
	UFUNCTION(BlueprintPure, Category = "Skill System")
	bool IsSkillAvailable(int32 SlotIndex) const;

	// 获取指定槽位装备的技能类型
	UFUNCTION(BlueprintPure, Category = "Skill System")
	ESkillType GetEquippedSkill(int32 SlotIndex) const;

	// UI调用：设置指定槽位的技能
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	void SetEquippedSkill(int32 SlotIndex, ESkillType NewSkill);

	// ========== AI技能系统接口 ==========

	// GameMode调用：启动AI技能系统（比赛开始后调用）
	UFUNCTION(BlueprintCallable, Category = "AI Skill System")
	void StartAISkillSystem();

	// ========== 难度系统接口 ==========

	// GameMode调用：应用特殊格子配置
	UFUNCTION(BlueprintCallable, Category = "Difficulty")
	void ApplySpecialAreas(const TArray<int32>& Indices, const TArray<ESlotEffectType>& Types);

	// GameMode调用：设置AI技能释放间隔
	UFUNCTION(BlueprintCallable, Category = "Difficulty")
	void SetAISkillInterval(float MinInterval, float MaxInterval);

	// ========== 测试函数（仅用于调试）==========

	// 测试：直接设置士气值
	UFUNCTION(BlueprintCallable, Category = "Morale System|Debug")
	void Debug_SetMorale(int32 NewMorale);

	// 测试：直接设置技能点
	UFUNCTION(BlueprintCallable, Category = "Morale System|Debug")
	void Debug_SetSkillPoints(int32 NewSkillPoints);

	// 测试：模拟消除N个方块
	UFUNCTION(BlueprintCallable, Category = "Morale System|Debug")
	void Debug_SimulateMatch(int32 TileCount, bool bIncludeSpecialBonus = false);

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

	// ========== 士气值系统事件 ==========

	// [事件] 士气值变化
	UFUNCTION(BlueprintImplementableEvent, Category = "Morale Events")
	void OnMoraleChanged(int32 NewMorale, int32 InMaxMorale, int32 AddedAmount);

	// [事件] 技能点变化
	UFUNCTION(BlueprintImplementableEvent, Category = "Morale Events")
	void OnSkillPointChanged(int32 NewSkillPoints, int32 InMaxSkillPoints);

	// ========== 龙舟竞速效果事件 ==========

	// [事件] 玩家触发加速效果（给玩家龙舟加速）
	// TriggerCount: 触发次数（消除了几个特殊格子）
	// SpeedBoostAmount: 加速数值
	UFUNCTION(BlueprintImplementableEvent, Category = "Race Events")
	void OnPlayerSpeedUpTriggered(int32 TriggerCount, float SpeedBoostAmount);

	// [事件] 玩家触发减速效果（给敌方龙舟减速）
	// TriggerCount: 触发次数
	// SlowDownAmount: 减速数值
	UFUNCTION(BlueprintImplementableEvent, Category = "Race Events")
	void OnPlayerSlowDownEnemyTriggered(int32 TriggerCount, float SlowDownAmount);

	// ========== 技能系统事件 ==========

	// [事件] 玩家技能释放成功
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Events")
	void OnSkillCasted(ESkillType SkillType, const FSkillConfig& Config);

	// [事件] AI释放技能
	// CasterAI: 施法的AI（AI1或AI2）
	// SkillType: 技能类型
	// TargetType: 目标类型（Self=增益给自己, Enemy=减益给敌人）
	// TargetAI: 如果是减益技能，目标AI是谁（AI1或AI2，如果目标是玩家则此参数无效）
	// bTargetIsPlayer: 如果是减益技能，目标是否是玩家（true=玩家, false=另一个AI）
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Events")
	void OnAISkillCasted(EAIBoatIndex CasterAI, ESkillType SkillType, ESkillTargetType TargetType, 
		EAIBoatIndex TargetAI, bool bTargetIsPlayer, const FSkillConfig& Config);

	// ========== 难度系统事件 ==========

	// [事件] 特殊格子配置已更新（UI需要刷新特殊格子显示）
	UFUNCTION(BlueprintImplementableEvent, Category = "Difficulty Events")
	void OnSpecialAreasUpdated();

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
	
	// 计算士气值奖励
	int32 CalculateMoraleReward(int32 TileCount, const TArray<FSpecialEffectData>& TriggeredEffects);

	// 检查并转换士气值为技能点
	void CheckMoraleToSkillPoint();

	// 生成棋盘
	void GenerateBoard();
	
	// 检查是否有可用移动
	bool HasAnyValidMove();

	// 触发龙舟竞速效果（内部使用）
	void TriggerRaceEffects(const TArray<FSpecialEffectData>& TriggeredEffects);

	// AI技能释放Timer
	void TriggerAISkill();

	// 调度下一次AI技能释放
	void ScheduleNextAISkill();

	// 获取技能的目标类型
	ESkillTargetType GetSkillTargetType(ESkillType SkillType) const;

	// 随机配置AI技能（当启用每局随机时调用）
	void RandomizeAISkills();
	
	// 待交换的索引
	int32 PendingSwapIndexA;
	int32 PendingSwapIndexB;

	// AI技能Timer句柄
	FTimerHandle AISkillTimerHandle;
};

