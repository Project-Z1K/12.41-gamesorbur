#pragma once
#include "ue.h"
#include "Looting.h"
#include "log.h"

vector<class Bot*> Bots{};
vector<class PlayerBot*> PlayerBots{};

enum PlooshAIPatrollingDirection
{
	Forward,
	Backward,
};

__forceinline double FVectorDistance(FVector a, FVector b) {
	FVector diff = b - a;
	double mchange = diff.X + diff.Y + diff.Z;

	if (mchange < 0) {
		mchange = -mchange;
	}

	return mchange;
}

class PlooshAIPatrollingComponent
{
public:
	ABP_PhoebePlayerController_C* PC;
	AFortAthenaPatrolPath* PatrolPath;
	int32 CurrentPatrolPointIndex = -1;
	PlooshAIPatrollingDirection PatrollingDirection = PlooshAIPatrollingDirection::Forward;


public:
	PlooshAIPatrollingComponent(ABP_PhoebePlayerController_C* PC)
	{
		this->PC = PC;
		this->PatrolPath = PC->CachedPatrollingComponent->PatrolPath;
		log_debug("Patrolling component: %s\n", PC->CachedPatrollingComponent->GetName().c_str());
		log_debug("Patrolling component addr: %p\n", PC->CachedPatrollingComponent);
		log_debug("Patrol path: %s\n", PatrolPath->GetName().c_str());
		log_debug("Patrol path addr: %p\n", PatrolPath);
	}

public:
	void DoPatrol() {
		if (!PatrolPath) return;
		auto IndexToUse = CurrentPatrolPointIndex == -1 ? 1 : CurrentPatrolPointIndex;
		if (CurrentPatrolPointIndex == 0) {
			PatrollingDirection = PlooshAIPatrollingDirection::Forward;
		}
		else if (CurrentPatrolPointIndex >= PatrolPath->PatrolPoints.Num()) {
			PatrollingDirection = PlooshAIPatrollingDirection::Backward;
		}
		AFortAthenaPatrolPoint* NextPoint = nullptr;
		if (PatrollingDirection == PlooshAIPatrollingDirection::Forward) {
			if (CurrentPatrolPointIndex == -1) {
				NextPoint = PatrolPath->PatrolPoints[IndexToUse];
			}
			else {
				NextPoint = PatrolPath->PatrolPoints[IndexToUse/* + 1*/];
			}
		}
		else {
			NextPoint = PatrolPath->PatrolPoints[IndexToUse/* - 1*/];
		}
		if (!NextPoint) return;
		auto DistanceToNextPoint = PC->PlayerBotPawn->GetDistanceTo(NextPoint);
		if (CurrentPatrolPointIndex != -1 && DistanceToNextPoint > 1) {
			log_debug("Distance: %f, Idx: %d, PPaths: %d\n", DistanceToNextPoint, IndexToUse, PatrolPath->PatrolPoints.Num());
			return;
		}
		else if (DistanceToNextPoint < 45) {
			if (PatrollingDirection == PlooshAIPatrollingDirection::Forward) {
				CurrentPatrolPointIndex++;
			}
			else {
				CurrentPatrolPointIndex--;
			}
		}
		log_debug("bleh\n");
		PC->MoveToActor(PatrolPath->PatrolPoints[IndexToUse], 0, true, false, true, nullptr, true);

		if (CurrentPatrolPointIndex == -1) {
			CurrentPatrolPointIndex = 1;
		}
	}
};

class Bot
{
public:
	ABP_PhoebePlayerController_C* PC;
	AFortPlayerPawnAthena* Pawn;
	bool bTickEnabled = false;
	AActor* CurrentTarget = nullptr;
	std::string CID = "";
	uint64_t tick_counter = 0;
	UFortWeaponItemDefinition* Weapon = nullptr;
	FGuid PickaxeGuid;
	UFortWeaponMeleeItemDefinition* Pickaxe = nullptr;
	FGuid WeaponGuid;
	bool jumping = false;
	int shotCounter = 0;
	FVector LastLocation{};
	bool equippedWeapon = true;
	AActor* FollowTarget;
	bool follow = true;
	EAlertLevel AlertLevel = EAlertLevel::Unaware;
	double MovementDistance = 0;
	FVector lastLocation{};
	PlooshAIPatrollingComponent* PatrollingComponent;
	AFortAthenaPatrolPath* PatrolPath;

public:
	Bot(AFortPlayerPawnAthena* Pawn)
	{
		this->Pawn = Pawn;
		PC = (ABP_PhoebePlayerController_C*)Pawn->Controller;
		Bots.push_back(this);
		/*auto BotPos = bot->Pawn->K2_GetActorLocation();
		float RandomX = GetMath()->RandomFloatInRange(-300.0f, 300.0f);
		float RandomY = GetMath()->RandomFloatInRange(-300.0f, 300.0f);

		FVector BotMove { BotPos.X + RandomX, BotPos.Y + RandomY, BotPos.Z };

		bot->PC->MoveToLocation(BotMove, 50, true, false, true, true, nullptr, true);*/
	}

public:
	FGuid GetItem(UFortItemDefinition* Def)
	{
		for (int32 /*size_t*/ i = 0; i < PC->Inventory->Inventory.ReplicatedEntries.Num(); i++)
		{
			if (PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition == Def)
				return PC->Inventory->Inventory.ReplicatedEntries[i].ItemGuid;
		}
		return FGuid();
	}

	void GiveItem(UFortItemDefinition* ODef, int Count = 1, bool equip = true)
	{
		UFortItemDefinition* Def = ODef;
		if (Def->GetName() == "AGID_Boss_Tina") {
			Def = StaticLoadObject<UFortItemDefinition>("/Game/Athena/Items/Weapons/Boss/WID_Boss_Tina.WID_Boss_Tina");
			//Def = StaticLoadObject<UFortItemDefinition>("/Game/Athena/Items/Gameplay/Boss/AGID_Boss_Tina.AGID_Boss_Tina");
		}
		UFortWorldItem* Item = (UFortWorldItem*)Def->CreateTemporaryItemInstanceBP(Count, 0);
		Item->OwnerInventory = PC->Inventory;

		if (Def->IsA(UFortWeaponRangedItemDefinition::StaticClass()))
		{
			Item->ItemEntry.LoadedAmmo = GetAmmoForDef((UFortWeaponItemDefinition*)Def);
		}

		PC->Inventory->Inventory.ReplicatedEntries.Add(Item->ItemEntry);
		PC->Inventory->Inventory.ItemInstances.Add(Item);
		PC->Inventory->Inventory.MarkItemDirty(Item->ItemEntry);
		PC->Inventory->HandleInventoryLocalUpdate();
		log_debug("Gave %s\n", Def->GetName().c_str());
		if (Def->GetName() == "WID_Harvest_Pickaxe_Athena_C_T01") {
			this->Pickaxe = (UFortWeaponMeleeItemDefinition*)Def;
			this->PickaxeGuid = GetItem(Def);
		}
		if (Def->IsA(UFortWeaponRangedItemDefinition::StaticClass()) && equip)
		{
			log_debug("Equipped %s\n", Def->GetName().c_str());
			this->Weapon = (UFortWeaponItemDefinition*)ODef;
			this->WeaponGuid = GetItem(Def);
			Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Def, GetItem(Def));
		}
	}

	// this was a failed attempt.
	/*virtual void OnPerceptionSensed(AActor* SourceActor, FAIStimulus& Stimulus)
	{
		if (Stimulus.bSuccessfullySensed == 1) {
			FHitResult HitResult;
			FVector StartLocation = this->Pawn->K2_GetActorLocation();
			FVector EndLocation = SourceActor->K2_GetActorLocation();

			if (!PC->LineOfSightTo(SourceActor, FVector(), true) && Pawn->GetDistanceTo(SourceActor) < 5000)
			{
				// No obstacles in the way, the AI bot can "see" the source actor
				CurrentTarget = SourceActor;
				log_info("Successfully sensed\n");
			}
		}

		if (Stimulus.bSuccessfullySensed == 1 && PC->LineOfSightTo(SourceActor, FVector(), true) && Pawn->GetDistanceTo(SourceActor) < 5000)
		{
			CurrentTarget = SourceActor;
			log_info("Successfully sensed\n");
		}
	}*/


	// OLD 
	virtual void OnPerceptionSensed(AActor* SourceActor, FAIStimulus& Stimulus)
	{
		//if (CurrentTarget != nullptr && CurrentTarget != SourceActor) return;
		/*float Distance = Pawn->GetDistanceTo(SourceActor);
		if (Pawn->bIsDBNO) return;
		if (Stimulus.Strength == 1.0f && (!Stimulus.bSuccessfullySensed || (Stimulus.bSuccessfullySensed && Distance > 500))) return;
		if (Stimulus.Strength != 1.0f && Distance < 1500 && PC->LineOfSightTo(SourceActor, FVector(), true)) {
			CurrentTarget = SourceActor;
			log_info("Successfully sensed (Hit)\n");
		}
		else if (Stimulus.bSuccessfullySensed == 1 && Distance < 3000) {
			CurrentTarget = SourceActor;
			log_info("Successfully sensed (Sense)\n");
		}
		else if (Stimulus.bSuccessfullySensed == 1 && PC->LineOfSightTo(SourceActor, FVector(), true) && Distance < 5000) {
			CurrentTarget = SourceActor;
			log_info("Successfully sensed (All)\n");
		}*/
		/*if (AlertLevel == EAlertLevel::Threatened || AlertLevel == EAlertLevel::LKP || AlertLevel == EAlertLevel::Alerted) {
			CurrentTarget = SourceActor;
			log_info("Successfully sensed\n");
		}*/
	}

	virtual void OnAlertLevelChanged(enum class EAlertLevel OldAlertLevel, enum class EAlertLevel NewAlertLevel)
	{
		AlertLevel = NewAlertLevel;
		if (&PC->TargetHandler && &PC->TargetHandler.Targets[0] && PC->TargetHandler.Targets[0].SourceActor != nullptr) {
			if (PC->TargetHandler.Targets[0].SourceActor->GetName().starts_with("BP_PlayerPawn_Athena_Phoebe_C_")) return;
			if (AlertLevel == EAlertLevel::Alerted || AlertLevel == EAlertLevel::Threatened || AlertLevel == EAlertLevel::LKP)
			{
				if (PC->TargetHandler.Targets[0].SourceActor) {
					CurrentTarget = PC->TargetHandler.Targets[0].SourceActor;
					if (AlertLevel == EAlertLevel::Threatened) {
						if (Weapon) {
							Pawn->EquipWeaponDefinition(Weapon, WeaponGuid);
							Pawn->PawnStopFire(0);
						}
					}

					if (AlertLevel == EAlertLevel::Alerted) {
						for (auto b : Bots) {
							if (b->PC == PC) continue;
							float Distance = Pawn->GetDistanceTo(b->Pawn);

							if (Distance < 1500) {
								b->PC->CurrentAlertLevel = AlertLevel;
								b->PC->OnAlertLevelChanged(b->AlertLevel, AlertLevel);
							}
						}
					}
				}
				log_info("Successfully sensed\n");
			}
			if (PC->TargetHandler.Targets[0].SupportingActor) {
				log_info("SUA: %s\n", PC->TargetHandler.Targets[0].SupportingActor->GetName().c_str());
			}
			if (PC->TargetHandler.Targets[0].SourceActor) {
				log_info("TA: %s\n", PC->TargetHandler.Targets[0].SourceActor->GetName().c_str());
			}
		}
		else {
			if (AlertLevel == EAlertLevel::Unaware) CurrentTarget = nullptr;
		}
		log_info("Alert level changed from %d to %d\n", (int)OldAlertLevel, (int)NewAlertLevel);
	}
};

class PlayerBot : public Bot
{
public:
	PlayerBot(AFortPlayerPawnAthena* Pawn) : Bot(Pawn)
	{
		log_debug("PB const called\n");
		this->Pawn = Pawn;
		PC = (ABP_PhoebePlayerController_C*)Pawn->Controller;
		PlayerBots.push_back(this);
	}
public:
	void OnPerceptionSensed(AActor* SourceActor, FAIStimulus& Stimulus) override
	{
		if (CurrentTarget->GetName().starts_with("BP_MangPlayerPawn_")/* || Pawn->GetName().starts_with("BP_PlayerPawn_Athena_Phoebe_C_")*/) {
			return;
		}
		float Distance = Pawn->GetDistanceTo(SourceActor);
		if (Pawn->bIsDBNO) return;
		if (Stimulus.Strength == 1.0f && (!Stimulus.bSuccessfullySensed || (Stimulus.bSuccessfullySensed && Distance > 500))) return;
		if (Stimulus.Strength != 1.0f && Distance < 1500 && PC->LineOfSightTo(SourceActor, FVector(), true)) {
			CurrentTarget = SourceActor;
			log_info("Successfully sensed (Hit)\n");
		}
		else if (Stimulus.bSuccessfullySensed == 1 && Distance < 3000) {
			CurrentTarget = SourceActor;
			log_info("Successfully sensed (Sense)\n");
		}
		else if (Stimulus.bSuccessfullySensed == 1 && PC->LineOfSightTo(SourceActor, FVector(), true) && Distance < 5000) {
			CurrentTarget = SourceActor;
			log_info("Successfully sensed (All)\n");
		}
	}


};
//UWorld
//TArray

// will bot shoot
int RandomNumGen() {
	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	int randomNumber = (std::rand() % 26) + 1;
	log_info("Random number between 1 and 3: %d\n", randomNumber);
	return randomNumber;
}

float ComputeRot(float Base, float Sep, float Max) {
	auto Num = GetMath()->RandomFloatInRange(0.f, 3.f);
	float Rot = 0.f;

	if (Num < .1f) {
		Rot = GetMath()->RandomFloatInRange(-Base, Base);
	}
	else if (Num > .1f && Num < 1.7f) {
		Rot = GetMath()->RandomFloatInRange(Base + Sep, Max);
	}
	else {
		Rot = GetMath()->RandomFloatInRange(-Max, -Base - Sep);
	}
	/*if (Num < 1.3f) {
		Rot = GetMath()->RandomFloatInRange(-Max, -Base - Sep);
	}
	else if (Num > 1.3f && Num < 1.7f) {
		Rot = GetMath()->RandomFloatInRange(-Base, Base);
	}
	else {
		Rot = GetMath()->RandomFloatInRange(Base + Sep, Max);
	}*/

	return Rot;
}

void TickBots()
{
	//AFortGameStateAthena
	//auto tt1 = high_resolution_clock::now();
	auto block = [](Bot* bot, std::function<void(Bot* bot)> const& SetUnaware, bool Alerted, bool Threatened, bool LKP) {
		/*if (bot->bTickEnabled && bot->Pawn) {
			auto OldLL = bot->LastLocation;
			bot->LastLocation = bot->Pawn->K2_GetActorLocation();
			auto LL = bot->LastLocation;

			auto ComputeVal = [](float oldC, float newC) {
				bool CN = newC > 0;
				bool OCN = oldC > 0;

				float val = CN ? (newC - oldC) : (OCN ? -(newC + oldC) : (-newC + oldC));
				return val;
			};
			auto Moved = ComputeVal(OldLL.X, LL.X) + ComputeVal(OldLL.Y, LL.Y);

			log_info("Moved: %f\n", Moved);
		}*/
		if (!bot->CurrentTarget && !bot->follow) {
			bot->follow = true;
		}
		if (bot->bTickEnabled && bot->Pawn && bot->FollowTarget && bot->follow) {
			auto BotPos = bot->Pawn->K2_GetActorLocation();
			auto TargetPos = bot->FollowTarget->K2_GetActorLocation();
			auto TestRot = GetMath()->FindLookAtRotation(BotPos, TargetPos);

			/*float RandomXmodLook = GetMath()->RandomFloatInRange(-60.0f, 70.0f);
			float RandomYmodLook = GetMath()->RandomFloatInRange(-80.0f, 80.0f);
			float RandomZmodLook = GetMath()->RandomFloatInRange(-70.0f, 70.0f);

			FVector TargetLookMod { TargetPos.X + RandomXmodLook, TargetPos.Y + RandomYmodLook, TargetPos.Z + RandomZmodLook };

			auto TestRotLook = GetMath()->FindLookAtRotation(BotPos, TargetLookMod);*/
			bot->PC->SetControlRotation(TestRot);
			bot->PC->K2_SetActorRotation(TestRot, true);
			bot->PC->MoveToActor(bot->FollowTarget, 50, true, false, true, nullptr, true);
			if (!bot->CurrentTarget && bot->PC->PlayerBotPawn && bot->FollowTarget && bot->PC->PlayerBotPawn->PlayerState && (AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)bot->FollowTarget)->PlayerState && ((AFortPlayerStateAthena*)bot->PC->PlayerBotPawn->PlayerState)->PlayerTeam != ((AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)bot->FollowTarget)->PlayerState)->PlayerTeam) {
				bot->CurrentTarget = bot->FollowTarget;
			}
		}
		//auto t1 = high_resolution_clock::now();
		if (bot->CurrentTarget && bot->PC->PlayerBotPawn && bot->PC->PlayerBotPawn->PlayerState && ((AFortPlayerPawnAthena*)bot->CurrentTarget)->PlayerState && ((AFortPlayerStateAthena*)bot->PC->PlayerBotPawn->PlayerState)->PlayerTeam == ((AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)bot->CurrentTarget)->PlayerState)->PlayerTeam) {
			bot->CurrentTarget = nullptr;
		}
		if (bot->CurrentTarget && ((AFortPlayerPawnAthena*)bot->CurrentTarget)->IsDead()) {
			bot->CurrentTarget = nullptr;
			bot->Pawn->PawnStopFire(0);
		}
		if (bot->bTickEnabled && bot->Pawn && !bot->Pawn->bIsDBNO && bot->CurrentTarget && !((AFortPlayerPawnAthena*)bot->CurrentTarget)->IsDead())
		{
			if (bot->Pawn->CurrentWeapon && bot->Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponItemDefinition::StaticClass()) /*could be a pickaxe*/)
			{
				auto BotPos = bot->Pawn->K2_GetActorLocation();
				auto TargetPos = bot->CurrentTarget->K2_GetActorLocation();
				float Distance = bot->Pawn->GetDistanceTo(bot->CurrentTarget);

				if (Alerted) {
					bot->Pawn->PawnStopFire(0);
					bot->PC->StopMovement();
					auto Rot = GetMath()->FindLookAtRotation(BotPos, TargetPos);
					bot->PC->SetControlRotation(Rot);
					bot->PC->K2_SetActorRotation(Rot, true);
					bot->tick_counter++;
					if (bot->tick_counter % 150 == 0 && bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true) && Distance < 2500) {
						bot->PC->CurrentAlertLevel = EAlertLevel::Threatened;
						bot->PC->OnAlertLevelChanged(bot->AlertLevel, EAlertLevel::Threatened);
					}
					return;
				}
				else if (Threatened/* && Distance < 2500*/) {
					if (Distance < 250 && bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true)) bot->PC->StopMovement();
					if (!bot->Pawn->bIsCrouched && GetMath()->RandomBoolWithWeight(0.01f)) {
						bot->Pawn->Crouch(false);
					}
					else if (GetMath()->RandomBoolWithWeight(0.01f)) {
						bot->Pawn->Jump();
						bot->jumping = true;
					}
					else if (bot->Pawn->bIsCrouched && (bot->tick_counter % 30) == 0) {
						bot->Pawn->UnCrouch(false);
					}
					else if (bot->jumping && (bot->tick_counter % 10) == 0) {
						bot->Pawn->StopJumping();
						bot->jumping = false;
					}
				}
				else if (/*!bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true) && Distance < 4000*/ LKP) {
					//bot->Pawn->PawnStopFire(0);
					auto LKPRun = [bot, BotPos, TargetPos]() {
						bot->Pawn->UnCrouch(false);
						bot->Pawn->StopJumping();
						bot->jumping = false;
						bot->PC->MoveToActor(bot->CurrentTarget, 150, true, false, true, nullptr, true);
						auto Rot = GetMath()->FindLookAtRotation((FVector&)BotPos, (FVector&)TargetPos);
						bot->PC->SetControlRotation(Rot);
						bot->PC->K2_SetActorRotation(Rot, true);
						};
					bot->tick_counter++;

					if (bot->tick_counter % 30 == 0) {
						if (bot->MovementDistance < 50) {
							if (bot->Pickaxe) {
								bot->Pawn->EquipWeaponDefinition(bot->Pickaxe, bot->PickaxeGuid);
								bot->Pawn->PawnStartFire(0);
							}
						}
						else {
							/*if (!bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true)) {
								bot->PC->CurrentAlertLevel = EAlertLevel::Alerted;
								bot->PC->OnAlertLevelChanged(bot->AlertLevel, EAlertLevel::Alerted);
							}*/
							if (bot->Weapon) {
								bot->Pawn->EquipWeaponDefinition(bot->Weapon, bot->WeaponGuid);
								bot->Pawn->PawnStopFire(0);
							}
						}

						bot->MovementDistance = 0;
					}
					else {
						// sum fvector
						FVector diff = BotPos - bot->lastLocation;
						double mchange = diff.X + diff.Y + diff.Z;
						bot->MovementDistance += mchange;
					}
					bot->lastLocation = BotPos;
					LKPRun();
					return;
				}
				else {
					bot->CurrentTarget = nullptr;
					SetUnaware(bot);
					bot->tick_counter++;
					return;
				}


				log_debug("%s's Distance to %s: %f\n", bot->Pawn->GetName().c_str(), bot->CurrentTarget->GetName().c_str(), Distance);

				std::string WeaponName = bot->Pawn->CurrentWeapon->Name.ToString();
				log_debug("%s is holding %s\n", bot->Pawn->GetName().c_str(), WeaponName.c_str());
				if (WeaponName.starts_with("B_Minigun_Athena_")) {
					if (bot->CID.starts_with("CID_NPC_Athena_Commando_M_")) {
						if (bot->shotCounter >= 24) {
							log_debug("Stopped Minigun fire on %s\n", WeaponName.c_str());
							bot->Pawn->PawnStopFire(0);
							bot->shotCounter = -40;
						}
					}
					else {
						if (bot->shotCounter >= 27) {
							log_debug("Stopped Minigun fire on %s\n", WeaponName.c_str());
							bot->Pawn->PawnStopFire(0);
							bot->shotCounter = -42;
						}
					}
				}
				else if (WeaponName.starts_with("B_Assault_MidasDrum_Athena_")) {
					if (bot->shotCounter >= /*45*/ 7) {
						log_debug("Stopped Drum Gun fire on %s\n", WeaponName.c_str());
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = /*-35*/ -23;
					}
				}
				else if (WeaponName.starts_with("B_Pistol_RapidFireSMG_Athena_")) {
					if (bot->shotCounter >= /*34*/ 15) {
						log_debug("Stopped Rapid fire on %s\n", WeaponName.c_str());
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = /*-10*/ -27;
					}
				}
				else if (WeaponName.starts_with("B_Pistol_Vigilante_Athena_")) {
					if (bot->shotCounter >= 12) {
						log_debug("Stopped Pistol fire on %s\n", WeaponName.c_str());
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -26;
					}
				}
				else if (WeaponName.starts_with("WID_Assault_Suppressed_Athena_")) {
					if (bot->shotCounter >= 17) {
						log_debug("Stopped Suppressed AR fire on %s\n", WeaponName.c_str());
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -22;
					}
				}
				else if (WeaponName.starts_with("B_DualPistol_Donut_Athena_")) {
					if (bot->shotCounter >= 15) {
						log_debug("Stopped Deadpool's fire on %s\n", WeaponName.c_str());
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -35;
					}
				}
				else if (WeaponName.starts_with("B_Assault_Auto_Zoom_SR_Child_Athena_")) {
					if (bot->shotCounter >= 3) {
						log_debug("Stopped Skye's fire on %s\n", WeaponName.c_str());
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -32;
					}
				}
				else if (bot->shotCounter >= 19) {
					log_debug("Stopped fire on %s\n", WeaponName.c_str());
					bot->Pawn->PawnStopFire(0);
					bot->shotCounter = -27;
				}

				if (//bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true) && 
					/*Distance < 2500 && */ Threatened) {

					//float RandomXmod = GetMath()->RandomFloatInRange(-XRange, XRange);
					//float RandomYmod = GetMath()->RandomFloatInRange(-YRange, YRange);
					//float RandomZmod = GetMath()->RandomFloatInRange(-ZRange, ZRange);
					FRotator TestRot;
					if (bot->tick_counter % 30 == 0) {
						float RandomXmod = ComputeRot(200.f, 80.f, 200.f);
						float RandomYmod = ComputeRot(240.f, 90.f, 240.f);
						float RandomZmod = ComputeRot(80.f, 100.f, 240.f);

						FVector TargetPosMod{ TargetPos.X + RandomXmod, TargetPos.Y + RandomYmod, TargetPos.Z + RandomZmod };

						TestRot = GetMath()->FindLookAtRotation(BotPos, TargetPosMod);
					}

					/*float RandomXmodLook = GetMath()->RandomFloatInRange(-60.0f, 70.0f);
					float RandomYmodLook = GetMath()->RandomFloatInRange(-80.0f, 80.0f);
					float RandomZmodLook = GetMath()->RandomFloatInRange(-70.0f, 70.0f);

					FVector TargetLookMod { TargetPos.X + RandomXmodLook, TargetPos.Y + RandomYmodLook, TargetPos.Z + RandomZmodLook };

					auto TestRotLook = GetMath()->FindLookAtRotation(BotPos, TargetLookMod);*/
					bot->follow = true;
					if (bot->tick_counter % 30 == 0) {
						bot->PC->SetControlRotation(TestRot);
						bot->PC->K2_SetActorRotation(TestRot, true);
					}
					bot->PC->MoveToActor(bot->CurrentTarget, 150, true, false, true, nullptr, true);
					//bot->Pawn->EquipBestWeapon();
					if (bot->tick_counter % 30 == 0) {
						if (bot->MovementDistance < 50) {
							if (bot->Pickaxe) {
								bot->Pawn->EquipWeaponDefinition(bot->Pickaxe, bot->PickaxeGuid);
								bot->Pawn->PawnStartFire(0);
							}
						}
						else {
							/*if (!bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true)) {
								bot->PC->CurrentAlertLevel = EAlertLevel::Alerted;
								bot->PC->OnAlertLevelChanged(bot->AlertLevel, EAlertLevel::Alerted);
							}*/
							if (bot->Weapon) {
								bot->Pawn->EquipWeaponDefinition(bot->Weapon, bot->WeaponGuid);
								log_debug("%s is firing at %s\n", bot->Pawn->GetName().c_str(), bot->CurrentTarget->GetName().c_str());
								if (bot->shotCounter >= 0) bot->Pawn->PawnStartFire(0);
							}
						}

						bot->MovementDistance = 0;
					}
					else {
						// sum fvector
						FVector diff = BotPos - bot->lastLocation;
						double mchange = diff.X + diff.Y + diff.Z;
						bot->MovementDistance += mchange;
					}
					bot->lastLocation = BotPos;
					if (bot->Weapon) bot->Pawn->EquipWeaponDefinition(bot->Weapon, bot->WeaponGuid);
					bot->shotCounter++;
				}
			}
		}
		else if (bot->bTickEnabled && bot->Pawn && bot->Pawn->bIsDBNO)
		{
			bot->CurrentTarget = nullptr;
			/*if (bot->tick_counter == 0 || (bot->tick_counter % 1800) == 0) {
				auto BotPos = bot->Pawn->K2_GetActorLocation();
				float RandomX = GetMath()->RandomFloatInRange(-500.0f, 500.0f);
				float RandomY = GetMath()->RandomFloatInRange(-500.0f, 500.0f);

				FVector BotMove { BotPos.X + RandomX, BotPos.Y + RandomY, BotPos.Z };

				bot->PC->MoveToLocation(BotMove, 25, true, false, true, true, nullptr, true);
				log_debug("random move DBNO: %lf %lf on %s\n", RandomX, RandomY, bot->CID.c_str());
			}*/
			//bot->PC->StopMovement();
		}
		else {
			/*if (bot->tick_counter == 0 || (bot->tick_counter % 1800) == 0) {
				auto BotPos = bot->Pawn->K2_GetActorLocation();
				float RandomX = GetMath()->RandomFloatInRange(-300.0f, 300.0f);
				float RandomY = GetMath()->RandomFloatInRange(-300.0f, 300.0f);

				FVector BotMove { BotPos.X + RandomX, BotPos.Y + RandomY, BotPos.Z };

				//bot->Pawn->K2_SetActorLocation(BotMove, false, nullptr, false);
				//bot->PC->MoveToLocation(BotMove, 50, true, false, true, true, nullptr, true);
				//bot->PC->MoveToActor(bot->Pawn, 50, true, false, true, nullptr, true);
				//bot->Pawn->K2_SetActorLocation(BotPos, false, nullptr, false);
				auto Actor = SpawnActor<AActor>(BotMove, GetMath()->FindLookAtRotation(BotPos, BotMove));
				//bot->Pawn->AddMovementInput(BotMove, 1.0, true);
				bot->PC->MoveToActor(Actor, 50, true, true, true, nullptr, true);
				log_debug("random move: %lf %lf on %s\n", RandomX, RandomY, bot->CID.c_str());
			}*/
			if (bot->PatrollingComponent) bot->PatrollingComponent->DoPatrol(); // not working :C
		}
		bot->tick_counter++;
		/*auto t2 = high_resolution_clock::now();
		auto time = duration_cast<milliseconds>(t2 - t1);
		log_debug("Ticked %s in %ldms.\n", bot->CID.c_str(), time.count());*/
		};
	/*auto tt2 = high_resolution_clock::now();
	auto time2 = duration_cast<milliseconds>(tt2 - tt1);
	log_debug("Ticked all bots in %ldms.\n", time2.count());*/
	auto SetUnaware = [](Bot* bot) {
		bot->PC->CurrentAlertLevel = EAlertLevel::Unaware;
		bot->PC->OnAlertLevelChanged(bot->AlertLevel, EAlertLevel::Unaware);
		bot->Pawn->PawnStopFire(0);
		bot->PC->StopMovement();
		};
	for (auto bot : Bots)
	{
		auto Alerted = bot->PC->CurrentAlertLevel == EAlertLevel::Alerted;
		auto Threatened = bot->PC->CurrentAlertLevel == EAlertLevel::Threatened;
		auto LKP = bot->PC->CurrentAlertLevel == EAlertLevel::LKP;
		block(bot, SetUnaware, Alerted, Threatened, LKP);
	}
	auto PBSetUnaware = [](Bot* bot) {};
	for (auto bot : PlayerBots)
	{
		float Distance = bot->Pawn->GetDistanceTo(bot->CurrentTarget);
		auto PBAlerted = false;
		auto PBThreatened = Distance < 2500;
		auto PBLKP = /*Distance < 4000*/ true;
		block(bot, PBSetUnaware, PBAlerted, PBThreatened, PBLKP);
	}
}

wchar_t* (*OnPerceptionSensedOG)(ABP_PhoebePlayerController_C* PC, AActor* SourceActor, FAIStimulus& Stimulus);
wchar_t* OnPerceptionSensed(ABP_PhoebePlayerController_C* PC, AActor* SourceActor, FAIStimulus& Stimulus)
{
	/*bool isBot = false;
	if (SourceActor && Cast<AFortPlayerPawnAthena>(SourceActor)->Controller && SourceActor->IsA(AFortPlayerPawnAthena::StaticClass()) && Cast<AFortPlayerPawnAthena>(SourceActor)->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass()) && PC->PlayerBotPawn && PC->PlayerBotPawn->PlayerState && ((AFortPlayerPawnAthena*)SourceActor)->PlayerState && ((AFortPlayerStateAthena*)PC->PlayerBotPawn->PlayerState)->PlayerTeam == ((AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)SourceActor)->PlayerState)->PlayerTeam) {
		isBot = true;
	}*/
	if (SourceActor->IsA(AFortPlayerPawnAthena::StaticClass()) && Cast<AFortPlayerPawnAthena>(SourceActor)->Controller /*!Cast<AFortPlayerPawnAthena>(SourceActor)->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass())*/
		// && PC->LineOfSightTo(SourceActor, FVector(), true)
		)
	{
		for (auto bot : Bots)
		{
			if (bot->PC == PC)
			{
				bot->OnPerceptionSensed(SourceActor, Stimulus);
			}
		}

		for (auto bot : PlayerBots)
		{
			if (bot->PC == PC)
			{
				bot->OnPerceptionSensed(SourceActor, Stimulus);
			}
		}
	}
	return OnPerceptionSensedOG(PC, SourceActor, Stimulus);
}

//void (*OnAlertLevelChangedOG)(ABP_PhoebePlayerController_C* PC, enum class EAlertLevel OldAlertLevel, enum class EAlertLevel NewAlertLevel);
void OnAlertLevelChanged(ABP_PhoebePlayerController_C* PC, enum class EAlertLevel OldAlertLevel, enum class EAlertLevel NewAlertLevel)
{
	/*bool isBot = false;
	if (SourceActor && Cast<AFortPlayerPawnAthena>(SourceActor)->Controller && SourceActor->IsA(AFortPlayerPawnAthena::StaticClass()) && Cast<AFortPlayerPawnAthena>(SourceActor)->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass()) && PC->PlayerBotPawn && PC->PlayerBotPawn->PlayerState && ((AFortPlayerPawnAthena*)SourceActor)->PlayerState && ((AFortPlayerStateAthena*)PC->PlayerBotPawn->PlayerState)->PlayerTeam == ((AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)SourceActor)->PlayerState)->PlayerTeam) {
		isBot = true;
	}*/
	for (auto bot : Bots)
	{
		if (bot->PC == PC)
		{
			bot->OnAlertLevelChanged(OldAlertLevel, NewAlertLevel);
		}
	}
	//return OnAlertLevelChangedOG(PC, OldAlertLevel, NewAlertLevel);
}

void (*OnPossessedPawnDiedOG)(ABP_PhoebePlayerController_C* PC, AActor* DamagedActor, float Damage, AController* InstigatedBy, AActor* DamageCauser, FVector HitLocation, UPrimitiveComponent* HitComp, FName BoneName, FVector Momentum);
void OnPossessedPawnDied(ABP_PhoebePlayerController_C* PC, AActor* DamagedActor, float Damage, AController* InstigatedBy, AActor* DamageCauser, FVector HitLocation, UPrimitiveComponent* HitComp, FName BoneName, FVector Momentum)
{
	Bot* KilledBot = nullptr;
	for (size_t i = 0; i < Bots.size(); i++)
	{
		auto bot = Bots[i];
		if (bot && bot->PC == PC)
		{
			if (bot->Pawn->GetName().starts_with("BP_Pawn_DangerGrape_")) {
				goto nodrop;
			}
			else {
				KilledBot = bot;
			}
		}
	}
	PC->PlayerBotPawn->SetMaxShield(0);
	log_info("I, K: %s, %s\n", InstigatedBy->GetName().c_str(), DamageCauser->GetName().c_str());
	for (int32 /*size_t*/ i = 0; i < PC->Inventory->Inventory.ReplicatedEntries.Num(); i++)
	{
		if (PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) || PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass()))
			continue;
		auto Def = PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition;
		if (Def->GetName() == "WID_Boss_Tina") {
			//Def = StaticLoadObject<UFortItemDefinition>("/Game/Athena/Items/Weapons/Boss/WID_Boss_Tina.WID_Boss_Tina");
			//Def = StaticLoadObject<UFortWeaponItemDefinition>("/Game/Athena/Items/Gameplay/Boss/AGID_Boss_Tina.AGID_Boss_Tina");
			Def = KilledBot->Weapon;
		}
		SpawnPickup(PC->Pawn->K2_GetActorLocation(), Def, PC->Inventory->Inventory.ReplicatedEntries[i].Count, PC->Inventory->Inventory.ReplicatedEntries[i].LoadedAmmo, EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::PlayerElimination);
		int Ammo = 0;
		int AmmoC = 0;
		if (Def->GetName() == "WID_Boss_Hos_MG") {
			Ammo = 60;
			AmmoC = 60;
		}
		else if (Def->GetName().starts_with("WID_Assault_LMG_Athena_")) {
			Ammo = 45;
			AmmoC = 45;
		}
		log_debug("%s\n", Def->GetName().c_str());
		if (Def->IsA(UFortWeaponRangedItemDefinition::StaticClass()))
		{
			///*UFortWeaponRangedItemDefinition*/ UFortWeaponItemDefinition* Def = (/*UFortWeaponRangedItemDefinition*/UFortWeaponItemDefinition*)PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition;
			UFortWeaponItemDefinition* Def2 = (UFortWeaponItemDefinition*)Def;
			SpawnPickup(PC->Pawn->K2_GetActorLocation(), Def2->GetAmmoWorldItemDefinition_BP(), AmmoC != 0 ? AmmoC : GetAmmoForDef(Def2), Ammo != 0 ? Ammo : GetAmmoForDef(Def2), EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::PlayerElimination);
		}
	}

nodrop:
	for (size_t i = 0; i < Bots.size(); i++)
	{
		auto bot = Bots[i];
		if (bot && bot->PC == PC)
		{
			Bots.erase(Bots.begin() + i);
			break;
		}
	}

	return OnPossessedPawnDiedOG(PC, DamagedActor, Damage, InstigatedBy, DamageCauser, HitLocation, HitComp, BoneName, Momentum);
}

inline bool ends_with(std::string const& value, std::string const& ending)
{
	if (ending.size() > value.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void spawnMeowscles();

AFortPlayerPawnAthena* (*SpawnBotOG)(UFortServerBotManagerAthena* BotManager, FVector SpawnLoc, FRotator SpawnRot, UFortAthenaAIBotCustomizationData* BotData, FFortAthenaAIBotRunTimeCustomizationData RuntimeBotData);
AFortPlayerPawnAthena* SpawnBot(UFortServerBotManagerAthena* BotManager, FVector SpawnLoc, FRotator SpawnRot, UFortAthenaAIBotCustomizationData* BotData, FFortAthenaAIBotRunTimeCustomizationData RuntimeBotData)
{
	spawnMeowscles();

	bool boss = true;
	std::string BotName = BotData->Name.ToString();
	log_info("Spawning %s, SI: %d\n", BotName.c_str(), RuntimeBotData.CustomSquadId);

	if (BotName.contains("MANG_POI_Yacht"))
	{
		BotData = StaticLoadObject<UFortAthenaAIBotCustomizationData>("/Game/Athena/AI/MANG/BotData/BotData_MANG_POI_HDP.BotData_MANG_POI_HDP");
	}

	if (BotData->CharacterCustomization->CustomizationLoadout.Character->GetName() == "CID_556_Athena_Commando_F_RebirthDefaultA")
	{
		/*if (POI.starts_with("MANG_POI_OilRig")) {
			BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_TacticalFishermanOil.CID_NPC_Athena_Commando_M_TacticalFishermanOil");
		} else if (POI.starts_with("MANG_POI_SJI")) {
			BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HeistSummerIsland.CID_NPC_Athena_Commando_M_HeistSummerIsland");
		} else if (POI.starts_with("MANG_POI_MountainBase")) {
			BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_PaddedArmorArctic.CID_NPC_Athena_Commando_M_PaddedArmorArctic");
		} else {*/
		std::string Tag = RuntimeBotData.PredefinedCosmeticSetTag.TagName.ToString();
		if (Tag == "Athena.Faction.Alter") {
			BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HenchmanBad.CID_NPC_Athena_Commando_M_HenchmanBad");
		}
		else /*if (POI.ends_with("_Ego"))*/ if (Tag == "Athena.Faction.Ego") {
			BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HenchmanGood.CID_NPC_Athena_Commando_M_HenchmanGood");
		}
		else {
			log_debug("GTag: %s\n", Tag.c_str());
		}
		boss = false;
		//}
	}/* else if (BotData->CharacterCustomization->CustomizationLoadout.Character->GetName() == "CID_693_Athena_Commando_M_BuffCat") {
		BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_693_Athena_Commando_M_BuffCat.CID_693_Athena_Commando_M_BuffCat_Good");
	}*/

	AActor* SpawnLocator = SpawnActor<ADefaultPawn>(SpawnLoc, SpawnRot);
	//UClass *PhoebePawnClass = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_PlayerPawn_Athena_Phoebe.BP_PlayerPawn_Athena_Phoebe_C");
	AFortPlayerPawnAthena* Ret = BotMutator->SpawnBot(BotData->PawnClass /*PhoebePawnClass*/, SpawnLocator, SpawnLoc, SpawnRot, true);
	AFortAthenaAIBotController* PC = (AFortAthenaAIBotController*)Ret->Controller;
	PC->CosmeticLoadoutBC = BotData->CharacterCustomization->CustomizationLoadout;
	for (int32 /*size_t*/ i = 0; i < BotData->CharacterCustomization->CustomizationLoadout.Character->HeroDefinition->Specializations.Num(); i++)
	{
		UFortHeroSpecialization* Spec = StaticLoadObject<UFortHeroSpecialization>(Conv_NameToString(BotData->CharacterCustomization->CustomizationLoadout.Character->HeroDefinition->Specializations[i].ObjectID.AssetPathName).ToString());

		if (Spec)
		{
			for (int32 /*size_t*/ i = 0; i < Spec->CharacterParts.Num(); i++)
			{
				UCustomCharacterPart* Part = StaticLoadObject<UCustomCharacterPart>(Conv_NameToString(Spec->CharacterParts[i].ObjectID.AssetPathName).ToString());
				Ret->ServerChoosePart(Part->CharacterPartType, Part);
			}
		}
	}

	Ret->CosmeticLoadout = BotData->CharacterCustomization->CustomizationLoadout;
	Ret->OnRep_CosmeticLoadout();

	log_debug("Character: %s\n", BotData->CharacterCustomization->CustomizationLoadout.Character->GetName().c_str());

	SpawnLocator->K2_DestroyActor();
	DWORD CustomSquadId = RuntimeBotData.CustomSquadId;
	BYTE TrueByte = 1;
	BYTE FalseByte = 0;
	BotManagerSetupStuffIdk(__int64(BotManager), __int64(Ret), __int64(BotData->BehaviorTree), 0, &CustomSquadId, 0, __int64(BotData->StartupInventory), __int64(BotData->BotNameSettings), 0, &FalseByte, 0, &TrueByte, RuntimeBotData);


	Bot* bot = new Bot(Ret);

	bot->CID = BotData->CharacterCustomization->CustomizationLoadout.Character->GetName();
	log_info("Spawned %s at %f %f %f.\n", bot->CID.c_str(), SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z);

	for (int32 /*size_t*/ i = 0; i < BotData->StartupInventory->Items.Num(); i++)
	{
		log_debug("Giving %s %s\n", bot->CID.c_str(), BotData->StartupInventory->Items[i]->GetName().c_str());
		bool equip = true;
		//if (BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Athena_FloppingRabbit"))
		//	continue;
		if (BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Athena_FloppingRabbit") || BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Boss_Adventure_GH")) {
			equip = false;
		}
		bot->GiveItem(BotData->StartupInventory->Items[i], 1, equip);
		if (auto Data = Cast<UFortWeaponItemDefinition>(BotData->StartupInventory->Items[i]))
		{
			if (Data->GetAmmoWorldItemDefinition_BP() && Data->GetAmmoWorldItemDefinition_BP() != Data)
			{
				bot->GiveItem(Data->GetAmmoWorldItemDefinition_BP(), 99999);
			}
		}
	}

	if ((BotName == "BD_DangerGrape_Default" || BotName.starts_with("BD_FrenchYedoc")) /*&& GetGameState()->GamePhase > EAthenaGamePhase::Aircraft */) {
		auto GM = GetGameMode();
		AFortPlayerPawn* closestPawn = nullptr;
		float closestDistance = 0;
		bool distanceSet = false;
		for (int32 /*size_t*/ i = 0; i < GM->AlivePlayers.Num(); i++)
		{
			auto PPC = GM->AlivePlayers[i];
			if (PPC && PPC->MyFortPawn)
			{
				float Distance = bot->Pawn->GetDistanceTo(PPC->MyFortPawn);
				if (!distanceSet) {
					distanceSet = true;
					closestDistance = Distance;
					closestPawn = PPC->MyFortPawn;
					continue;
				}
				else {
					if (Distance < closestDistance) {
						closestDistance = Distance;
						closestPawn = PPC->MyFortPawn;
					}
				}
			}
		}

		if (closestPawn && distanceSet) {
			bot->FollowTarget = closestPawn;
			bot->CurrentTarget = closestPawn;
		}
	}

	TArray<AFortAthenaPatrolPath*> PatrolPaths;

	GetStatics()->GetAllActorsOfClass(GetWorld(), AFortAthenaPatrolPath::StaticClass(), (TArray<AActor*>*) & PatrolPaths);

	for (int i = 0; i < PatrolPaths.Num(); i++) {
		if (PatrolPaths[i]->PatrolPoints[0]->K2_GetActorLocation() == SpawnLoc) {
			bot->PC->CachedPatrollingComponent->SetPatrolPath(PatrolPaths[i]);
		}
	}


	//bot->PatrollingComponent = new PlooshAIPatrollingComponent(bot->PC);

	bot->bTickEnabled = true;
	return Ret;
}

AFortPlayerPawnAthena* SpawnPlayerBot(UFortServerBotManagerAthena* BotManager, FVector SpawnLoc, FRotator SpawnRot, UFortAthenaAIBotCustomizationData* BotData, FFortAthenaAIBotRunTimeCustomizationData RuntimeBotData)
{
	std::string BotName = BotData->Name.ToString();
	log_info("Spawning player bot\n");


	BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HenchmanGood.CID_NPC_Athena_Commando_M_HenchmanGood");

	AActor* SpawnLocator = SpawnActor<ADefaultPawn>(SpawnLoc, SpawnRot);
	UClass* PhoebePawnClass = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_PlayerPawn_Athena_Phoebe.BP_PlayerPawn_Athena_Phoebe_C");
	AFortPlayerPawnAthena* Ret = BotMutator->SpawnBot(PhoebePawnClass, SpawnLocator, SpawnLoc, SpawnRot, true);
	AFortAthenaAIBotController* PC = (AFortAthenaAIBotController*)Ret->Controller;
	PC->CosmeticLoadoutBC = BotData->CharacterCustomization->CustomizationLoadout;
	for (int32 /*size_t*/ i = 0; i < BotData->CharacterCustomization->CustomizationLoadout.Character->HeroDefinition->Specializations.Num(); i++)
	{
		UFortHeroSpecialization* Spec = StaticLoadObject<UFortHeroSpecialization>(Conv_NameToString(BotData->CharacterCustomization->CustomizationLoadout.Character->HeroDefinition->Specializations[i].ObjectID.AssetPathName).ToString());

		if (Spec)
		{
			for (int32 /*size_t*/ i = 0; i < Spec->CharacterParts.Num(); i++)
			{
				UCustomCharacterPart* Part = StaticLoadObject<UCustomCharacterPart>(Conv_NameToString(Spec->CharacterParts[i].ObjectID.AssetPathName).ToString());
				Ret->ServerChoosePart(Part->CharacterPartType, Part);
			}
		}
	}

	Ret->CosmeticLoadout = BotData->CharacterCustomization->CustomizationLoadout;
	Ret->OnRep_CosmeticLoadout();

	log_debug("Character: %s\n", BotData->CharacterCustomization->CustomizationLoadout.Character->GetName().c_str());

	SpawnLocator->K2_DestroyActor();
	DWORD CustomSquadId = RuntimeBotData.CustomSquadId;
	BYTE TrueByte = 1;
	BYTE FalseByte = 0;
	BotManagerSetupStuffIdk(__int64(BotManager), __int64(Ret), __int64(BotData->BehaviorTree), 0, &CustomSquadId, 0, __int64(BotData->StartupInventory), __int64(BotData->BotNameSettings), 0, &FalseByte, 0, &TrueByte, RuntimeBotData);

	PlayerBot* bot = new PlayerBot(Ret);

	bot->CID = BotData->CharacterCustomization->CustomizationLoadout.Character->GetName();

	for (int32 /*size_t*/ i = 0; i < BotData->StartupInventory->Items.Num(); i++)
	{
		bool equip = true;
		//if (BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Athena_FloppingRabbit"))
		//	continue;
		if (BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Athena_FloppingRabbit") || BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Boss_Adventure_GH")) {
			equip = false;
		}
		bot->GiveItem(BotData->StartupInventory->Items[i], 1, equip);
		if (auto Data = Cast<UFortWeaponItemDefinition>(BotData->StartupInventory->Items[i]))
		{
			if (Data->GetAmmoWorldItemDefinition_BP() && Data->GetAmmoWorldItemDefinition_BP() != Data)
			{
				bot->GiveItem(Data->GetAmmoWorldItemDefinition_BP(), 99999);
			}
		}
	}

	bot->bTickEnabled = true;
	return Ret;
}

void spawnMeowscles() {
	static bool meowsclesSpawned = false;
	if (meowsclesSpawned) return;
	meowsclesSpawned = true;

	UFortAthenaAIBotCustomizationData* customization = StaticLoadObject<UFortAthenaAIBotCustomizationData>("/Game/Athena/AI/MANG/BotData/BotData_MANG_POI_HMW");
	FFortAthenaAIBotRunTimeCustomizationData runtimeData{};
	runtimeData.CustomSquadId = 0;
	FVector SpawnLocs[] = {
		// default
		{
			-68996.0,
			80660.0,
			6152.0
		},

		// patrol path starts
		{
			-70912.0,
			81376.0,
			5684.0
		},
		{
			-70532.0,
			79440.0,
			5684.0
		},
		{
			-68708.0,
			79440.0,
			5684.0
		},
		{
			-68708.0,
			78524.0,
			5684.0
		},
		{
			-67696.0,
			81540.0,
			5672.0
		},
		{
			-69728.0,
			81376.0,
			5684.0
		}
	};

	FRotator Rotation = {
		0.0,
		-179.9999f,
		0.0
	};

	/*std::random_device rd;
	std::seed_seq seed{ rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd() };
	std::mt19937_64 e1(rd());
	std::uniform_int_distribution<int> uniform_dist(0, (sizeof(SpawnLocs) / sizeof(FVector)) - 1);*/
	float random = GetMath()->RandomFloatInRange(0.f, (sizeof(SpawnLocs) / sizeof(FVector)) - 1);
	log_debug("SpawnLocs %lld\n", (sizeof(SpawnLocs) / sizeof(FVector)) - 1);

	auto Meowscles = SpawnBot(GetGameMode()->ServerBotManager, SpawnLocs[(int)random], Rotation, customization, runtimeData);
	if (!Meowscles) {
		return;
	}
	Meowscles->SetMaxShield(400);
	Meowscles->SetShield(400);
}
FVector FindBotSpawn(AFortAthenaMapInfo* MapInfo, FVector Center, float Radius)
{
	static FVector* (*PickSupplyDropLocationOriginal)(AFortAthenaMapInfo * MapInfo, FVector * outLocation, __int64 Center, float Radius) = decltype(PickSupplyDropLocationOriginal)(__int64(GetModuleHandleA(0)) + 0x18848f0);

	if (!PickSupplyDropLocationOriginal)
		return FVector(0, 0, 0);

	// LOG_INFO(LogDev, "GetAircraftDropVolume: {}", __int64(GetAircraftDropVolume()));

	FVector Out = FVector(0, 0, 0);
	auto ahh = PickSupplyDropLocationOriginal(MapInfo, &Out, __int64(&Center), Radius);
	return Out;
}


bool spawned = false;

void SpawnBotsAtPlayerStarts() {
	if (spawned) return;
	auto MI = GetGameState()->MapInfo;
	int numPlayers = GetGameState()->GameMemberInfoArray.Members.Num();
#ifdef MineekPrivate
	int bots = 99;
#else
	int bots = 50;
#endif
	UBehaviorTree* botTree = StaticLoadObject<UBehaviorTree>("/Game/Athena/AI/Phoebe/BehaviorTrees/BT_Phoebe");
	FVector SpawnIslandLoc = { 179899, -176186, -2611 };
	UClass* PhoebePawn = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_PlayerPawn_Athena_Phoebe.BP_PlayerPawn_Athena_Phoebe_C");
	auto customization = StaticLoadObject<UFortAthenaAIBotCustomizationData>("/Game/Athena/AI/MANG/BotData/BotData_MANG_POI_HMW");
	auto BT = customization->BehaviorTree;
	auto PCL = customization->PawnClass;
	customization->BehaviorTree = botTree;
	customization->PawnClass = PhoebePawn;
	customization->Name = Conv_StringToName(L"PlooshAI");

	for (int i = 0; i < bots; i++) {
		//UFortAthenaAIBotCustomizationData* customization = new UFortAthenaAIBotCustomizationData();

		//->SkillLevel = 0.5;

		FFortAthenaAIBotRunTimeCustomizationData runtimeData;
		runtimeData.CustomSquadId = 3 + numPlayers + i;
		FRotator RandomYawRotator{};
		RandomYawRotator.Yaw = (float)rand() * 0.010986663f;

		int Radius = 100000;
		FVector Location = FindBotSpawn(MI, FVector(1, 1, 10000), (float)Radius);

		auto Llama = SpawnActor<AFortAthenaSupplyDrop>(MI->LlamaClass.Get(), Location, RandomYawRotator);

		auto GroundLocation = Llama->FindGroundLocationAt(Location);

		Llama->K2_DestroyActor();

		SpawnPlayerBot(GetGameMode()->ServerBotManager, GroundLocation, RandomYawRotator, customization, runtimeData);
	}

	customization->BehaviorTree = BT;
	customization->PawnClass = PCL;
	customization->Name = Conv_StringToName(L"Meowscles");
	spawned = true;
}
