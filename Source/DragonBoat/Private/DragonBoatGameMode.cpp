// Fill out your copyright notice in the Description page of Project Settings.

#include "DragonBoatGameMode.h"
#include "Datamanagement.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ADragonBoatGameMode::ADragonBoatGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// 默认配置
	RaceTrackLength = 10000.0f;
	StartLinePosition = FVector(0.0f, 0.0f, 0.0f);
	FinishLinePosition = FVector(10000.0f, 0.0f, 0.0f);
	CountdownDuration = 3.0f;
	ProgressUpdateInterval = 0.2f;  // 默认每0.2秒更新一次
	RaceEndDelay = 5.0f;

	// 运行时数据初始化
	CurrentGameState = ERaceGameState::PreRace;
	CurrentRaceTime = 0.0f;
	FinishedBoatCount = 0;
	CountdownRemaining = 0;

	PlayerBoat = nullptr;
	AIBoat1 = nullptr;
	AIBoat2 = nullptr;
}

void ADragonBoatGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 初始化龙舟数据数组
	BoatDataArray.SetNum(3);

	UE_LOG(LogTemp, Log, TEXT("DragonBoatGameMode: Initialized"));
}

void ADragonBoatGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 只在比赛进行中更新计时
	if (CurrentGameState == ERaceGameState::Racing)
	{
		CurrentRaceTime += DeltaTime;
	}

	// 其他逻辑全部使用Timer，不在Tick中执行
}

// ========================================
// 公共接口
// ========================================

void ADragonBoatGameMode::StartCountdown()
{
	if (CurrentGameState != ERaceGameState::PreRace)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartCountdown: Can only start countdown in PreRace state!"));
		return;
	}

	CurrentGameState = ERaceGameState::PreRace;
	CountdownRemaining = FMath::CeilToInt(CountdownDuration);

	UE_LOG(LogTemp, Log, TEXT("StartCountdown: Starting countdown from %d seconds"), CountdownRemaining);

	// 设置重复Timer，每秒触发一次
	GetWorld()->GetTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&ADragonBoatGameMode::CountdownTick,
		1.0f,  // 每秒触发
		true   // 重复
	);

	// 立即触发第一次倒计时
	CountdownTick();
}

void ADragonBoatGameMode::StartRace()
{
	CurrentGameState = ERaceGameState::Racing;
	CurrentRaceTime = 0.0f;
	FinishedBoatCount = 0;

	// 重置龙舟数据
	for (int32 i = 0; i < BoatDataArray.Num(); i++)
	{
		BoatDataArray[i] = FBoatRaceData();
	}

	UE_LOG(LogTemp, Log, TEXT("StartRace: Race started!"));

	OnRaceStarted();

	// 初始化数据管理器，并启动AI技能系统
	ADatamanagement* DataMgmt = Cast<ADatamanagement>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ADatamanagement::StaticClass()));
	if (DataMgmt)
	{
		DataMgmt->StartAISkillSystem();
		UE_LOG(LogTemp, Log, TEXT("StartRace: AI Skill System activated"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("StartRace: Datamanagement not found! AI skills will not work."));
	}

	// 启动进度更新Timer
	GetWorld()->GetTimerManager().SetTimer(
		ProgressUpdateTimerHandle,
		this,
		&ADragonBoatGameMode::UpdateProgress,
		ProgressUpdateInterval,
		true  // 重复
	);
}

void ADragonBoatGameMode::PauseRace()
{
	if (CurrentGameState != ERaceGameState::Racing)
		return;

	CurrentGameState = ERaceGameState::Paused;

	// 暂停进度更新
	GetWorld()->GetTimerManager().PauseTimer(ProgressUpdateTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("PauseRace: Race paused"));
}

void ADragonBoatGameMode::ResumeRace()
{
	if (CurrentGameState != ERaceGameState::Paused)
		return;

	CurrentGameState = ERaceGameState::Racing;

	// 恢复进度更新
	GetWorld()->GetTimerManager().UnPauseTimer(ProgressUpdateTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("ResumeRace: Race resumed"));
}

int32 ADragonBoatGameMode::GetBoatRank(int32 BoatIndex) const
{
	if (BoatDataArray.IsValidIndex(BoatIndex))
	{
		return BoatDataArray[BoatIndex].CurrentRank;
	}
	return 1;
}

float ADragonBoatGameMode::GetBoatProgress(int32 BoatIndex) const
{
	if (BoatDataArray.IsValidIndex(BoatIndex))
	{
		return BoatDataArray[BoatIndex].CurrentProgress;
	}
	return 0.0f;
}

// ========================================
// 内部函数
// ========================================

void ADragonBoatGameMode::CountdownTick()
{
	UE_LOG(LogTemp, Log, TEXT("CountdownTick: %d"), CountdownRemaining);

	OnCountdownTick(CountdownRemaining);

	CountdownRemaining--;

	if (CountdownRemaining < 0)
	{
		// 倒计时结束，停止Timer并开始比赛
		GetWorld()->GetTimerManager().ClearTimer(CountdownTimerHandle);
		StartRace();
	}
}

void ADragonBoatGameMode::UpdateProgress()
{
	if (CurrentGameState != ERaceGameState::Racing)
		return;

	TArray<AActor*> Boats = { PlayerBoat, AIBoat1, AIBoat2 };
	TArray<int32> OldRanks;

	// 保存旧排名用于检测变化
	for (const FBoatRaceData& Data : BoatDataArray)
	{
		OldRanks.Add(Data.CurrentRank);
	}

	// 1. 更新每条龙舟的进度
	for (int32 i = 0; i < Boats.Num(); i++)
	{
		if (!Boats[i] || BoatDataArray[i].bHasFinished)
			continue;

		// 计算进度（基于X坐标位置）
		float BoatX = Boats[i]->GetActorLocation().X;
		float StartX = StartLinePosition.X;
		float FinishX = FinishLinePosition.X;

		float Progress = FMath::Clamp(
			(BoatX - StartX) / (FinishX - StartX),
			0.0f, 1.0f
		);

		BoatDataArray[i].CurrentProgress = Progress;

		// 检测是否完成
		if (Progress >= 1.0f && !BoatDataArray[i].bHasFinished)
		{
			OnBoatReachedFinish(i);
		}
	}

	// 2. 计算排名
	UpdateRankings();

	// 3. 检测排名变化
	for (int32 i = 0; i < BoatDataArray.Num(); i++)
	{
		if (OldRanks.IsValidIndex(i) && OldRanks[i] != BoatDataArray[i].CurrentRank)
		{
			UE_LOG(LogTemp, Log, TEXT("Rank Changed: Boat %d from rank %d to %d"), 
				i, OldRanks[i], BoatDataArray[i].CurrentRank);
			OnRankChanged(i, OldRanks[i], BoatDataArray[i].CurrentRank);
		}
	}

	// 4. 通知UI更新进度
	TArray<float> Progresses;
	TArray<int32> Ranks;
	for (const FBoatRaceData& Data : BoatDataArray)
	{
		Progresses.Add(Data.CurrentProgress);
		Ranks.Add(Data.CurrentRank);
	}

	OnProgressUpdated(Progresses, Ranks);
}

void ADragonBoatGameMode::UpdateRankings()
{
	// 按进度排序（进度越高排名越前）
	TArray<int32> SortedIndices = { 0, 1, 2 };

	SortedIndices.Sort([this](int32 A, int32 B) {
		// 已完成的优先
		if (BoatDataArray[A].bHasFinished != BoatDataArray[B].bHasFinished)
			return BoatDataArray[A].bHasFinished;

		// 已完成的按完成时间排序
		if (BoatDataArray[A].bHasFinished && BoatDataArray[B].bHasFinished)
			return BoatDataArray[A].FinishTime < BoatDataArray[B].FinishTime;

		// 未完成的按进度降序
		return BoatDataArray[A].CurrentProgress > BoatDataArray[B].CurrentProgress;
	});

	// 分配排名
	for (int32 Rank = 0; Rank < SortedIndices.Num(); Rank++)
	{
		int32 BoatIndex = SortedIndices[Rank];
		BoatDataArray[BoatIndex].CurrentRank = Rank + 1;
	}
}

void ADragonBoatGameMode::OnBoatReachedFinish(int32 BoatIndex)
{
	BoatDataArray[BoatIndex].bHasFinished = true;
	BoatDataArray[BoatIndex].FinishTime = CurrentRaceTime;
	FinishedBoatCount++;

	// 确定最终排名（基于完成时间）
	int32 FinalRank = FinishedBoatCount;

	UE_LOG(LogTemp, Log, TEXT("Boat %d finished! Time: %.2f, Rank: %d"), 
		BoatIndex, CurrentRaceTime, FinalRank);

	OnBoatFinished(BoatIndex, CurrentRaceTime, FinalRank);

	// 如果是第一名完成，启动结束倒计时
	if (FinishedBoatCount == 1)
	{
		UE_LOG(LogTemp, Log, TEXT("First boat finished! Starting end timer (%.1f seconds)"), RaceEndDelay);

		GetWorld()->GetTimerManager().SetTimer(
			RaceEndTimerHandle,
			this,
			&ADragonBoatGameMode::EndRace,
			RaceEndDelay,  // 延迟X秒
			false
		);
	}

	// 或者所有龙舟都完成立即结束
	if (FinishedBoatCount >= 3)
	{
		UE_LOG(LogTemp, Log, TEXT("All boats finished! Ending race immediately"));

		GetWorld()->GetTimerManager().ClearTimer(RaceEndTimerHandle);
		EndRace();
	}
}

void ADragonBoatGameMode::EndRace()
{
	CurrentGameState = ERaceGameState::Finished;

	// 停止进度更新
	GetWorld()->GetTimerManager().ClearTimer(ProgressUpdateTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("EndRace: Race finished!"));

	// 构建最终结果
	TArray<FBoatFinalResult> FinalRankings;
	for (int32 i = 0; i < BoatDataArray.Num(); i++)
	{
		FBoatFinalResult Result;
		Result.BoatIndex = i;
		Result.FinalRank = BoatDataArray[i].CurrentRank;
		Result.FinishTime = BoatDataArray[i].FinishTime;
		Result.bIsPlayer = (i == 0);

		FinalRankings.Add(Result);

		UE_LOG(LogTemp, Log, TEXT("  Boat %d: Rank %d, Time %.2f"), 
			i, Result.FinalRank, Result.FinishTime);
	}

	// 按排名排序
	FinalRankings.Sort([](const FBoatFinalResult& A, const FBoatFinalResult& B) {
		return A.FinalRank < B.FinalRank;
	});

	OnRaceFinished(FinalRankings);
}
