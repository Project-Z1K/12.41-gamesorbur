#pragma once
#include "ue.h"
#include "PlayerState.h"
#include "log.h"

float EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY,
	const FString& ContextString = FString(), EEvaluateCurveTableResult* OutResult = nullptr)
{
	static auto fn = StaticLoadObject<UFunction>("/Script/Engine.DataTableFunctionLibrary.EvaluateCurveTableRow");

	float wtf{};
	EEvaluateCurveTableResult wtf1{};

	struct { UCurveTable* CurveTable; FName RowName; float InXY; EEvaluateCurveTableResult OutResult; float OutXY; FString ContextString; }
	UDataTableFunctionLibrary_EvaluateCurveTableRow_Params{ CurveTable, RowName, InXY, wtf1, wtf, ContextString };

	static auto DefaultClass = UDataTableFunctionLibrary::StaticClass();
	DefaultClass->ProcessEvent(fn, &UDataTableFunctionLibrary_EvaluateCurveTableRow_Params);

	if (OutResult)
		*OutResult = UDataTableFunctionLibrary_EvaluateCurveTableRow_Params.OutResult;

	return UDataTableFunctionLibrary_EvaluateCurveTableRow_Params.OutXY;

	float wtf{};
	//EEvaluateCurveTableResult wtf1{};
	struct FCurveTableRowHandle RH {
		CurveTable, RowName
	};
	return GetFortKismet()->EvaluateCurveTableRow(RH, InXY, &wtf, ContextString);
}

#define log_info(str, ...) _log_base(str, _color_info, "Info", ##__VA_ARGS__)


FVector PickSupplyDropLocation(AFortAthenaMapInfo* MapInfo, FVector Center, float Radius)
{
	static FVector* (*PickSupplyDropLocationOriginal)(AFortAthenaMapInfo * MapInfo, FVector * outLocation, __int64 Center, float Radius) = decltype(PickSupplyDropLocationOriginal)(__int64(GetModuleHandleA(0)) + 0x18848f0);

	if (!PickSupplyDropLocationOriginal)
		return FVector(0, 0, 0);

	// LOG_INFO(LogDev, "GetAircraftDropVolume: {}", __int64(GetAircraftDropVolume()));

	FVector Out = FVector(0, 0, 0);
	auto ahh = PickSupplyDropLocationOriginal(MapInfo, &Out, __int64(&Center), Radius);
	return Out;
}

static inline int CalcuateCurveMinAndMax(FScalableFloat Min, FScalableFloat Max, float Multiplier = 100.f) // returns 000 not 0.00 (forgot techinal name for this)
{
	float MinSpawnPercent = EvaluateCurveTableRow(Min.Curve.CurveTable, Min.Curve.RowName, 0);
	float MaxSpawnPercent = EvaluateCurveTableRow(Max.Curve.CurveTable, Max.Curve.RowName, 0);
}


void SpawnLlamas()
{
	auto MapInfo = GetGameState()->MapInfo;
	int AmountOfLlamasSpawned = 0;
	//auto AmountOfLlamasToSpawn = CalcuateCurveMinAndMax(MapInfo->LlamaQuantityMin, MapInfo->LlamaQuantityMax, 1);
	auto AmountOfLlamasToSpawn = 5;

	log_info("Attempting to spawn %d llamas.\n", AmountOfLlamasToSpawn);

	//FVector SpawnIslandLoc = { 179899, -176186, -2611 };

	for (int i = 0; i < AmountOfLlamasToSpawn/* + 1*/; i++)
	{
		int Radius = 100000;
		FVector Location = PickSupplyDropLocation(MapInfo, FVector(1, 1, 10000), (float)Radius);
		//if (i == 5) Location = SpawnIslandLoc;

		// LOG_INFO(LogDev, "Initial Llama at {} {} {}", Location.X, Location.Y, Location.Z);

		/*if (Location.CompareVectors(FVector(0, 0, 0)))
			continue;*/

		FRotator RandomYawRotator{};
		RandomYawRotator.Yaw = (float)rand() * 0.010986663f;

		FTransform InitialSpawnTransform{};
		InitialSpawnTransform.Translation = Location;
		InitialSpawnTransform.Rotation = FRotToQuat(RandomYawRotator);
		InitialSpawnTransform.Scale3D = FVector(1, 1, 1);

		auto LlamaStart = SpawnActor<AFortAthenaSupplyDrop>(MapInfo->LlamaClass.Get(), Location, RandomYawRotator);

		// LOG_INFO(LogDev, "LlamaStart: {}", __int64(LlamaStart));

		if (!LlamaStart)
			continue;

		auto GroundLocation = LlamaStart->FindGroundLocationAt(InitialSpawnTransform.Translation);

		LlamaStart->K2_DestroyActor();

		//FTransform FinalSpawnTransform = InitialSpawnTransform;
		//FinalSpawnTransform.Translation = GroundLocation;

		log_debug("Spawning Llama #%d at %f %f %f\n", i, GroundLocation.X, GroundLocation.Y, GroundLocation.Z);

		//GetStatics()->FinishSpawningActor(LlamaStart, FinalSpawnTransform);
		auto Llama = SpawnActor<AFortAthenaSupplyDrop>(MapInfo->LlamaClass.Get(), GroundLocation, RandomYawRotator);

		Llama->bCanBeDamaged = false;

		if (!Llama)
			continue;
		AmountOfLlamasSpawned++;
	}

	log_info("Spawned %d llamas.\n", AmountOfLlamasSpawned);
}
