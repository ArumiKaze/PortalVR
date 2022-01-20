#include "PortalCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

APortalCharacter::APortalCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	VROrigin->SetupAttachment(RootComponent);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
}

void APortalCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APortalCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APortalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UENUM(BlueprintType)
enum class ModName: uint8
{
	IronHelmet,
	IronPants,
	IronShoes,
	IronGloves,
	IronChestPlate,
	RandomUpgrade1,
	RandomUpgrade2
};

class AUpgradeItem : public AActor
{
	//public for the sake of simplicity, normally use getter
public:
	ModName upgradeName;
};

//assume we have 25 upgrades item objects in this array
TArray<AUpgradeItem> arrayUpgradeMods;

void APlayer::CheckModSet()
{
	//store upgrade names in a hashtable
	TSet<ModName> setModNames;
	for (const auto& mod: arrayUpgradeMods)
	{
		setModNames.Emplace(mod.upgradeName);
	}

	//checks for iron skin set
	if (setModNames.Contains(ModName::IronHelmet) && setModNames.Contains(ModName::IronPants) && setModNames.Contains(ModName::IronShoes)
		&& setModNames.Contains(ModName::IronGloves) && setModNames.Contains(ModName::IronChestPlate))
	{
		ActivateIronSkinBonus();
	}
	else
	{
		DeactivateIronSkinBonus();
	}

	//checks for random skin set
	if (setModNames.Contains(ModName::RandomUpgrade1) && setModNames.Contains(ModName::RandomUpgrade2))
	{
		ActivateRandomBonus();
		return;
	}
	else
	{
		DeactivateRandomBonus();
	}
}