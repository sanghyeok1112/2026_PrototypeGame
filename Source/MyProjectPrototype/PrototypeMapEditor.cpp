#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "LevelEditorViewport.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/TargetPoint.h"
#include "Engine/PointLight.h"
#include "Engine/BlockingVolume.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "UObject/SavePackage.h"

namespace PrototypeMap
{
static const TCHAR* MapPath=TEXT("/Game/Prototype/Maps/Lvl_DefenseBlockout");

static UMaterialInstanceConstant* Material(const TCHAR* Name,FLinearColor Color)
{
    const FString PackageName=FString(TEXT("/Game/Prototype/Materials/"))+Name;
    const FString ObjectPath=PackageName+TEXT(".")+Name;
    if (UMaterialInstanceConstant* Existing=LoadObject<UMaterialInstanceConstant>(nullptr,*ObjectPath)) return Existing;
    UMaterialInstanceConstant* Result=NewObject<UMaterialInstanceConstant>(CreatePackage(*PackageName),Name,RF_Public|RF_Standalone);
    Result->SetParentEditorOnly(LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
    Result->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")),Color);
    Result->PostEditChange();
    FAssetRegistryModule::AssetCreated(Result);
    Result->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
    UPackage::SavePackage(Result->GetOutermost(),Result,*FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension()),Args);
    return Result;
}

static AStaticMeshActor* Shape(UWorld* World,const FString& Name,FVector Position,FVector Size,
    UMaterialInterface* Mat,bool bCollision=true,float Yaw=0.f,bool bCylinder=false)
{
    AStaticMeshActor* Actor=World->SpawnActor<AStaticMeshActor>();
    Actor->SetActorLabel(Name);
    Actor->SetFolderPath(TEXT("Blockout/Geometry"));
    Actor->SetMobility(EComponentMobility::Movable);
    UStaticMeshComponent* Mesh=Actor->GetStaticMeshComponent();
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,bCylinder ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetMaterial(0,Mat);
    Mesh->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->SetCanEverAffectNavigation(bCollision);
    Actor->SetActorLocation(Position);
    Actor->SetActorRotation(FRotator(0,Yaw,0));
    Actor->SetActorScale3D(Size/100.f);
    Actor->SetMobility(EComponentMobility::Static);
    return Actor;
}

static void Label(UWorld* World,const FString& Name,const FString& Text,FVector Position,float Yaw,float Size=90.f)
{
    AActor* Actor=World->SpawnActor<AActor>();
    Actor->SetActorLabel(Name);
    Actor->SetFolderPath(TEXT("Blockout/Signs"));
    UTextRenderComponent* Component=NewObject<UTextRenderComponent>(Actor,TEXT("SignText"));
    Actor->AddInstanceComponent(Component);
    Actor->SetRootComponent(Component);
    Component->SetText(FText::FromString(Text));
    Component->SetHorizontalAlignment(EHTA_Center);
    Component->SetVerticalAlignment(EVRTA_TextCenter);
    Component->SetWorldSize(Size);
    Component->SetTextRenderColor(FColor(245,237,211));
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCanEverAffectNavigation(false);
    Component->RegisterComponent();
    Actor->SetActorLocation(Position);
    Actor->SetActorRotation(FRotator(0,Yaw,0));
}

static void Create()
{
    if (!GEditor || GEditor->PlayWorld) return;
    if (FPackageName::DoesPackageExist(MapPath))
    {
        UE_LOG(LogTemp,Error,TEXT("PROTOTYPE_MAP map already exists; refusing to overwrite %s"),MapPath);
        return;
    }
    UWorld* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World) return;
    UMaterialInterface* Sand=Material(TEXT("MI_BlockoutSand"),FLinearColor(.43f,.31f,.19f));
    UMaterialInterface* Road=Material(TEXT("MI_BlockoutRoad"),FLinearColor(.22f,.17f,.115f));
    UMaterialInterface* Stone=Material(TEXT("MI_BlockoutStone"),FLinearColor(.24f,.25f,.26f));
    UMaterialInterface* Wood=Material(TEXT("MI_BlockoutWood"),FLinearColor(.22f,.11f,.055f));
    UMaterialInterface* Aircraft=Material(TEXT("MI_BlockoutAircraft"),FLinearColor(.07f,.25f,.34f));
    UMaterialInterface* Colony=Material(TEXT("MI_BlockoutColony"),FLinearColor(.62f,.16f,.04f));

    Shape(World,TEXT("Ground_120m"),FVector(0,0,-50),FVector(12000,12000,100),Sand);
    Shape(World,TEXT("Central_Defense_Plaza"),FVector(0,0,2),FVector(2600,2600,4),Stone,false);
    Shape(World,TEXT("East_West_Road"),FVector(0,0,4),FVector(11000,800,4),Road,false);
    Shape(World,TEXT("North_South_Road"),FVector(0,0,5),FVector(800,11000,4),Road,false);

    // Recognizable aircraft silhouette, all pieces remain ordinary editable actors.
    Shape(World,TEXT("Aircraft_Fuselage"),FVector(0,0,140),FVector(800,200,240),Aircraft);
    Shape(World,TEXT("Aircraft_Wings"),FVector(-80,0,120),FVector(220,1050,40),Aircraft);
    Shape(World,TEXT("Aircraft_TailWing"),FVector(-350,0,180),FVector(130,450,30),Aircraft);
    Shape(World,TEXT("Aircraft_TailFin"),FVector(-350,0,290),FVector(130,30,240),Aircraft);
    Shape(World,TEXT("Aircraft_Nose"),FVector(420,0,130),FVector(100,150,160),Aircraft);
    Label(World,TEXT("Aircraft_Sign"),TEXT("DEFENSE CORE"),FVector(0,-180,400),-90.f,80.f);

    const FVector BuildingPositions[]={FVector(-1800,-1500,0),FVector(1800,-1500,0),FVector(-1800,1500,0),FVector(1800,1500,0)};
    const TCHAR* BuildingNames[]={TEXT("WORKSHOP"),TEXT("CLINIC"),TEXT("SALOON"),TEXT("SUPPLIES")};
    for(int32 I=0;I<4;++I)
    {
        const FVector P=BuildingPositions[I];
        const float Yaw=P.Y<0 ? 0.f : 180.f;
        const FRotator R(0,Yaw,0);
        auto Local=[&](FVector V){ return P+R.RotateVector(V); };
        Shape(World,FString::Printf(TEXT("Town_%d_BackWall"),I),Local(FVector(0,-350,180)),FVector(800,40,360),Wood,true,Yaw);
        Shape(World,FString::Printf(TEXT("Town_%d_LeftWall"),I),Local(FVector(-380,0,180)),FVector(40,700,360),Wood,true,Yaw);
        Shape(World,FString::Printf(TEXT("Town_%d_RightWall"),I),Local(FVector(380,0,180)),FVector(40,700,360),Wood,true,Yaw);
        Shape(World,FString::Printf(TEXT("Town_%d_SignBeam"),I),Local(FVector(0,350,330)),FVector(800,40,60),Wood,true,Yaw);
        Label(World,FString::Printf(TEXT("Town_%d_Label"),I),BuildingNames[I],Local(FVector(0,380,325)),Yaw+90.f,65.f);
    }

    const FVector ColonyPositions[]={FVector(4500,0,0),FVector(0,4500,0),FVector(-4500,0,0)};
    const TCHAR* ColonyNames[]={TEXT("EAST COLONY"),TEXT("NORTH COLONY"),TEXT("WEST COLONY")};
    for(int32 I=0;I<3;++I)
    {
        const FVector P=ColonyPositions[I];
        Shape(World,FString::Printf(TEXT("Colony_%d_Area"),I),P+FVector(0,0,8),FVector(1200,1200,4),Road,false);
        Shape(World,FString::Printf(TEXT("Colony_%d_Base"),I),P+FVector(0,0,55),FVector(450,450,110),Colony,true,0,true);
        Shape(World,FString::Printf(TEXT("Colony_%d_Core"),I),P+FVector(0,0,240),FVector(180,180,300),Colony);
        const FVector TowardCenter=(-P).GetSafeNormal();
        Label(World,FString::Printf(TEXT("Colony_%d_Label"),I),ColonyNames[I],P+TowardCenter*250+FVector(0,0,470),TowardCenter.Rotation().Yaw,95.f);
        for(int32 Side : {-1,1})
            Shape(World,FString::Printf(TEXT("Colony_%d_Cover_%d"),I,Side),P+FVector(450,Side*450,65),FVector(250,130,130),Stone);
    }
    Shape(World,TEXT("MineGate_Left"),FVector(-400,-4600,300),FVector(180,240,600),Stone);
    Shape(World,TEXT("MineGate_Right"),FVector(400,-4600,300),FVector(180,240,600),Stone);
    Shape(World,TEXT("MineGate_Top"),FVector(0,-4600,620),FVector(980,240,160),Stone);
    Label(World,TEXT("MineGate_Label"),TEXT("MINE"),FVector(0,-4450,610),90.f,110.f);

    for(int32 Axis=0;Axis<2;++Axis)
        for(int32 Side : {-1,1})
            for(int32 Distance : {2200,3300})
            {
                FVector P=Axis==0 ? FVector(Side*Distance,580,75) : FVector(580,Side*Distance,75);
                Shape(World,FString::Printf(TEXT("RoadCover_%d_%d_%d"),Axis,Side,Distance),P,FVector(280,140,150),Wood,true,Axis*90.f);
                P=Axis==0 ? FVector(Side*(Distance+300),-650,55) : FVector(-650,Side*(Distance+300),55);
                Shape(World,FString::Printf(TEXT("RoadLowCover_%d_%d_%d"),Axis,Side,Distance),P,FVector(180,180,110),Stone);
            }
    Shape(World,TEXT("Boundary_East"),FVector(6050,0,150),FVector(100,12200,300),Stone);
    Shape(World,TEXT("Boundary_West"),FVector(-6050,0,150),FVector(100,12200,300),Stone);
    Shape(World,TEXT("Boundary_North"),FVector(0,6050,150),FVector(12000,100,300),Stone);
    Shape(World,TEXT("Boundary_South"),FVector(0,-6050,150),FVector(12000,100,300),Stone);

    APlayerStart* Start=World->SpawnActor<APlayerStart>(FVector(-600,-900,110),FRotator(0,65,0));
    Start->SetActorLabel(TEXT("PlayerStart_DefensePlaza"));
    World->GetWorldSettings()->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
    // A single native/BP enemy is placed away from the safe spawn for movement/combat checks.
    if(UClass* MonsterClass=LoadClass<APawn>(nullptr,TEXT("/Game/Monster/BP_Monster.BP_Monster_C")))
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AActor* Monster=World->SpawnActor<AActor>(MonsterClass,FVector(2000,-900,110),FRotator(0,180,0),Params);
        if(Monster){ Monster->SetActorLabel(TEXT("Combat_Test_Monster")); Monster->SetFolderPath(TEXT("Blockout/CombatTest")); }
    }

    ADirectionalLight* Sun=World->SpawnActor<ADirectionalLight>(FVector(0,0,4000),FRotator(-48,-35,0));
    Sun->SetActorLabel(TEXT("Sun"));
    Sun->GetComponent()->SetIntensity(5.f);
    World->SpawnActor<ASkyAtmosphere>();
    ASkyLight* Sky=World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetIntensity(1.f);
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->RecaptureSky();
    AExponentialHeightFog* Fog=World->SpawnActor<AExponentialHeightFog>();
    Fog->GetComponent()->SetFogDensity(.003f);

    FNavigationSystem::AddNavigationSystemToWorld(*World,FNavigationSystemRunMode::EditorMode);
    ANavMeshBoundsVolume* Volume=World->SpawnActor<ANavMeshBoundsVolume>();
    Volume->SetActorLabel(TEXT("NavBounds_EntireBlockout"));
    UCubeBuilder* Builder=NewObject<UCubeBuilder>(Volume);
    Builder->X=12000; Builder->Y=12000; Builder->Z=2400;
    UActorFactory::CreateBrushForVolumeActor(Volume,Builder);
    Volume->SetActorLocation(FVector(0,0,600));
    Volume->PostEditChange();
    if(UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
    {
        Nav->OnNavigationBoundsUpdated(Volume);
        Nav->ReleaseInitialBuildingLock();
        Nav->Build();
        if(ANavigationData* Data=Nav->GetDefaultNavDataInstance(FNavigationSystem::Create))
        {
            Data->RebuildAll();
            Data->EnsureBuildCompletion();
        }
        int32 ValidRoutes=0;
        for(const FVector& P:ColonyPositions)
        {
            UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(World,FVector(-600,-900,0),P+(-P).GetSafeNormal()*500.f);
            const bool Valid=Path && Path->IsValid() && !Path->IsPartial();
            if(Valid)++ValidRoutes;
            UE_LOG(LogTemp,Display,TEXT("PROTOTYPE_MAP route %s valid=%d"),*P.ToString(),Valid);
        }
        UE_LOG(LogTemp,Display,TEXT("PROTOTYPE_MAP reachable_colonies=%d/3"),ValidRoutes);
    }
    for(FLevelEditorViewportClient* View:GEditor->GetLevelViewportClients())
        if(View && View->IsPerspective())
        {
            View->SetViewLocation(FVector(7800,-7800,7800));
            View->SetViewRotation(FRotator(-35.264f,135.f,0));
            View->Invalidate();
        }
    World->MarkPackageDirty();
    const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,MapPath);
    UE_LOG(LogTemp,Display,TEXT("PROTOTYPE_MAP COMPLETE saved=%d map=%s"),Saved,MapPath);
    if(FParse::Param(FCommandLine::Get(),TEXT("PrototypeMapBatch")))
        FPlatformMisc::RequestExit(false);
}

static FAutoConsoleCommand CreateCommand(TEXT("PrototypeMap.Create"),TEXT("Create a new editable defense blockout map; refuses to overwrite an existing map."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){ Create(); return false; }),2.f);
    }));

static void Finalize()
{
    UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if(!World || World->IsGameWorld() || !World->GetOutermost()->GetName().StartsWith(TEXT("/Game/Prototype/Maps/Lvl_")))
    { UE_LOG(LogTemp,Error,TEXT("PROTOTYPE_MAP finalize requires the new blockout map")); return; }
    const bool IsMine=World->GetOutermost()->GetName().Contains(TEXT("DesignMine"));
    if(IsMine)
    {
        auto RepairRamp=[World](const FString& Name,FVector A,FVector B)
        {
            const FVector Delta=B-A;
            for(TActorIterator<AStaticMeshActor> It(World);It;++It)
            {
                const FString Label=It->GetActorLabel();
                if(Label!=Name && !Label.StartsWith(Name+TEXT("_Rail_"))) continue;
                const bool Rail=Label!=Name;
                const int32 Side=Label.EndsWith(TEXT("_-1")) ? -1 : 1;
                It->Modify(); It->SetMobility(EComponentMobility::Movable);
                It->SetActorLocation((A+B)*.5f+(Rail ? FVector(0,Side*350,100) : FVector(0,0,-50)));
                It->SetActorRotation(Delta.Rotation());
                It->SetActorScale3D(FVector(Delta.Size(),Rail?70.f:650.f,Rail?180.f:100.f)/100.f);
                It->SetMobility(EComponentMobility::Static);
                It->PostEditMove(true);
            }
        };
        // Only small overlaps at the landings: long overlaps leave a >35cm drop
        // where the flat landing ends and make the navmesh islands disconnected.
        RepairRamp(TEXT("Mine_Descent_EntranceToUpper"),FVector(880,0,0),FVector(2620,0,-800));
        RepairRamp(TEXT("Mine_Descent_UpperToLower"),FVector(4380,0,-800),FVector(7120,0,-2400));
    }
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if(!Nav) { UE_LOG(LogTemp,Error,TEXT("PROTOTYPE_MAP missing navigation system")); return; }
    FlushAsyncLoading();
    Nav->ReleaseInitialBuildingLock();
    Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock);
    Nav->Build();
    if(ANavigationData* Data=Nav->GetDefaultNavDataInstance(FNavigationSystem::Create))
    { Data->RebuildAll(); Data->EnsureBuildCompletion(); }
    int32 ValidRoutes=0;
    const TArray<FVector> Goals=IsMine ? TArray<FVector>{FVector(3500,0,-800),FVector(8100,0,-2400)}
        : TArray<FVector>{FVector(4000,0,0),FVector(0,4000,0),FVector(-4000,0,0)};
    if(IsMine) for(float X:{0.f,800.f,1700.f,2700.f,3500.f,4300.f,5800.f,7300.f,8100.f})
    {
        FHitResult Floor;
        World->LineTraceSingleByChannel(Floor,FVector(X,0,1000),FVector(X,0,-4000),ECC_Visibility);
        FNavLocation Projected;
        const bool OnNav=Nav->ProjectPointToNavigation(Floor.ImpactPoint,Projected,FVector(100,100,300));
        UE_LOG(LogTemp,Display,TEXT("DESIGN_MINE_PROBE X=%.0f floor=%s actor=%s nav=%d projected=%s"),
            X,*Floor.ImpactPoint.ToString(),*GetNameSafe(Floor.GetActor()),OnNav,*Projected.Location.ToString());
    }
    for(const FVector& Goal:Goals)
    {
        UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(World,IsMine ? FVector(-500,0,0) : FVector(-600,-900,0),Goal);
        const bool Valid=Path && Path->IsValid() && !Path->IsPartial();
        if(Valid)++ValidRoutes;
        UE_LOG(LogTemp,Display,TEXT("PROTOTYPE_MAP FINAL route %s valid=%d"),*Goal.ToString(),Valid);
    }
    const FString SavePath=World->GetOutermost()->GetName();
    const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,SavePath);
    UE_LOG(LogTemp,Display,TEXT("PROTOTYPE_MAP FINAL saved=%d reachable=%d/%d"),Saved,ValidRoutes,Goals.Num());
    if(FParse::Param(FCommandLine::Get(),TEXT("PrototypeMapBatch"))) FPlatformMisc::RequestExit(false);
}
static FAutoConsoleCommand FinalizeCommand(TEXT("PrototypeMap.Finalize"),TEXT("Rebuild and save navigation in the new blockout after map loading completes."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){ Finalize(); return false; }),2.f);
    }));

// Design revision: slides 24 and 34-39. Coordinates/sizes are blockout choices,
// not measurements inferred from the reference screenshots.
static void Marker(UWorld* W,const FString& Name,FVector P,FName Kind,FName Region)
{
    ATargetPoint* Point=W->SpawnActor<ATargetPoint>(P,FRotator::ZeroRotator);
    Point->SetActorLabel(Name);
    Point->SetFolderPath(TEXT("Design/GameplayMarkers"));
    Point->Tags={Kind,Region};
}

static bool BuildSaveDesign(UWorld* W,const TCHAR* Path,FVector BoundsCenter,FVector BoundsSize,
    FVector RouteStart,const TArray<FVector>& Goals)
{
    FNavigationSystem::AddNavigationSystemToWorld(*W,FNavigationSystemRunMode::EditorMode);
    ANavMeshBoundsVolume* Volume=W->SpawnActor<ANavMeshBoundsVolume>();
    Volume->SetActorLabel(TEXT("NavBounds_DesignMap"));
    UCubeBuilder* Builder=NewObject<UCubeBuilder>(Volume);
    Builder->X=BoundsSize.X; Builder->Y=BoundsSize.Y; Builder->Z=BoundsSize.Z;
    UActorFactory::CreateBrushForVolumeActor(Volume,Builder);
    Volume->SetActorLocation(BoundsCenter);
    Volume->PostEditChange();
    FlushAsyncLoading();
    int32 Valid=0;
    if(UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
    {
        Nav->OnNavigationBoundsUpdated(Volume);
        Nav->ReleaseInitialBuildingLock();
        Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock);
        Nav->Build();
        if(ANavigationData* Data=Nav->GetDefaultNavDataInstance(FNavigationSystem::Create))
        { Data->RebuildAll(); Data->EnsureBuildCompletion(); }
        for(const FVector& Goal:Goals)
        {
            UNavigationPath* Route=UNavigationSystemV1::FindPathToLocationSynchronously(W,RouteStart,Goal);
            const bool OK=Route && Route->IsValid() && !Route->IsPartial();
            Valid+=OK ? 1 : 0;
            UE_LOG(LogTemp,Display,TEXT("DESIGN_MAP %s route=%s valid=%d"),Path,*Goal.ToString(),OK);
        }
    }
    W->MarkPackageDirty();
    const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(W,Path);
    UE_LOG(LogTemp,Display,TEXT("DESIGN_MAP SAVED %s saved=%d routes=%d/%d"),Path,Saved,Valid,Goals.Num());
    return Saved && Valid==Goals.Num();
}

static void DesignLighting(UWorld* W)
{
    ADirectionalLight* Sun=W->SpawnActor<ADirectionalLight>(FVector(0,0,6000),FRotator(-45,-25,0));
    Sun->GetComponent()->SetIntensity(6.f);
    W->SpawnActor<ASkyAtmosphere>();
    ASkyLight* Sky=W->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.f);
    Sky->GetLightComponent()->RecaptureSky();
    W->SpawnActor<AExponentialHeightFog>()->GetComponent()->SetFogDensity(.002f);
}

static void DesignGameMode(UWorld* W,FVector Start,FRotator Rotation)
{
    W->GetWorldSettings()->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
    W->SpawnActor<APlayerStart>(Start,Rotation)->SetActorLabel(TEXT("PlayerStart_DesignMap"));
}

static void DesignBuilding(UWorld* W,const FString& Name,FVector P,float Yaw,FVector Size,UMaterialInterface* Wood)
{
    const FRotator R(0,Yaw,0);
    auto Local=[&](FVector V){return P+R.RotateVector(V);};
    const float H=400.f, HalfX=Size.X*.5f, HalfY=Size.Y*.5f;
    Shape(W,Name+TEXT("_Back"),Local(FVector(0,-HalfY,H*.5f)),FVector(Size.X,40,H),Wood,true,Yaw);
    Shape(W,Name+TEXT("_Left"),Local(FVector(-HalfX,0,H*.5f)),FVector(40,Size.Y,H),Wood,true,Yaw);
    Shape(W,Name+TEXT("_Right"),Local(FVector(HalfX,0,H*.5f)),FVector(40,Size.Y,H),Wood,true,Yaw);
    Shape(W,Name+TEXT("_Facade"),Local(FVector(0,HalfY,370)),FVector(Size.X,50,100),Wood,true,Yaw);
    Shape(W,Name+TEXT("_Porch"),Local(FVector(0,HalfY+160,5)),FVector(Size.X+120,320,10),Wood,false,Yaw);
    Label(W,Name+TEXT("_Sign"),Name,Local(FVector(0,HalfY+35,370)),Yaw+90,70);
    Marker(W,Name+TEXT("_QuestPoint"),Local(FVector(0,0,100)),TEXT("TownService"),FName(*Name));
}

static bool CreateVillage()
{
    const TCHAR* Path=TEXT("/Game/Prototype/Maps/Lvl_DesignVillage");
    if(FPackageName::DoesPackageExist(Path)) { UE_LOG(LogTemp,Error,TEXT("DESIGN_MAP exists, not overwriting %s"),Path); return false; }
    UWorld* W=UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UMaterialInterface* Sand=Material(TEXT("MI_BlockoutSand"),FLinearColor(.43f,.31f,.19f));
    UMaterialInterface* Road=Material(TEXT("MI_BlockoutRoad"),FLinearColor(.22f,.17f,.115f));
    UMaterialInterface* Stone=Material(TEXT("MI_BlockoutStone"),FLinearColor(.24f,.25f,.26f));
    UMaterialInterface* Wood=Material(TEXT("MI_BlockoutWood"),FLinearColor(.22f,.11f,.055f));
    UMaterialInterface* Blue=Material(TEXT("MI_BlockoutAircraft"),FLinearColor(.07f,.25f,.34f));
    Shape(W,TEXT("Island_PlayArea_200m"),FVector(0,0,-50),FVector(20000,20000,100),Sand);
    Shape(W,TEXT("Village_MainStreet"),FVector(0,0,3),FVector(1100,19000,6),Road,false);
    Shape(W,TEXT("Village_CrossStreet"),FVector(0,0,4),FVector(19000,900,6),Road,false);
    Shape(W,TEXT("Defense_Plaza"),FVector(0,-650,6),FVector(2700,2200,8),Stone,false);
    Shape(W,TEXT("Aircraft_Wreck_Body"),FVector(0,-650,125),FVector(900,210,230),Blue);
    Shape(W,TEXT("Aircraft_Wreck_Wing"),FVector(-100,-650,130),FVector(200,1150,40),Blue);
    Shape(W,TEXT("Aircraft_Wreck_Tail"),FVector(-390,-650,290),FVector(130,40,300),Blue);
    Label(W,TEXT("DefenseCoreSign"),TEXT("AIRCRAFT WRECK"),FVector(0,-850,440),-90,85);
    Marker(W,TEXT("DefenseCore_Anchor"),FVector(0,-650,120),TEXT("DefenseCore"),TEXT("Village"));
    DesignBuilding(W,TEXT("SALOON"),FVector(-1900,1500,0),-90,FVector(1100,1000,0),Wood);
    DesignBuilding(W,TEXT("CLINIC"),FVector(1900,1500,0),90,FVector(850,900,0),Wood);
    DesignBuilding(W,TEXT("WORKSHOP"),FVector(-2000,-1400,0),-90,FVector(1100,1100,0),Wood);
    DesignBuilding(W,TEXT("FOOD STORE"),FVector(2000,-1500,0),90,FVector(900,1000,0),Wood);
    // Four broad gates preserve separate routes for defense and excursions.
    for(int32 Side:{-1,1}) for(int32 Segment:{-1,1})
    {
        Shape(W,FString::Printf(TEXT("VillageFence_NS_%d_%d"),Side,Segment),FVector(Segment*1800,Side*3200,90),FVector(2800,50,180),Wood);
        Shape(W,FString::Printf(TEXT("VillageFence_EW_%d_%d"),Side,Segment),FVector(Side*3200,Segment*1800,90),FVector(50,2800,180),Wood);
    }
    Shape(W,TEXT("Watchtower_Platform"),FVector(2700,2300,615),FVector(650,650,30),Wood);
    for(int32 I=0;I<30;++I)
        Shape(W,FString::Printf(TEXT("Watchtower_Step_%02d"),I),FVector(2700,820+I*40,(I+1)*10),FVector(240,45,(I+1)*20),Wood);
    Label(W,TEXT("Watchtower_Label"),TEXT("WATCHTOWER"),FVector(2700,2300,820),-90,65);
    Marker(W,TEXT("Watchtower_UnlockPoint"),FVector(2700,2300,720),TEXT("UnlockPoint"),TEXT("Village"));

    const TCHAR* Names[]={TEXT("NORTH"),TEXT("EAST"),TEXT("SOUTH"),TEXT("WEST")};
    const float Yaws[]={0.f,-90.f,180.f,90.f};
    TArray<FVector> Goals;
    for(int32 I=0;I<4;++I)
    {
        const FRotator R(0,Yaws[I],0);
        auto Local=[&](FVector P){return R.RotateVector(P);};
        const FName Region(*FString(Names[I]));
        const FString Prefix=FString(TEXT("Outer_"))+Names[I];
        Label(W,Prefix+TEXT("_GateSign"),FString(Names[I])+TEXT(" OUTSKIRTS"),Local(FVector(0,3500,340)),Yaws[I]-90.f,100);
        const FVector Candidates[]={FVector(-900,5600,100),FVector(850,6700,100),FVector(-700,7900,100)};
        for(int32 J=0;J<3;++J)
            Marker(W,FString::Printf(TEXT("ColonyCandidate_%s_%d"),Names[I],J+1),Local(Candidates[J]),TEXT("ColonyCandidate"),Region);
        Marker(W,Prefix+TEXT("_BossArena"),Local(FVector(0,7000,100)),TEXT("BossArena"),Region);
        Marker(W,Prefix+TEXT("_QuestEvent"),Local(FVector(1500,7600,100)),TEXT("QuestEvent"),Region);
        Goals.Add(Local(FVector(0,7000,0)));
        // Each approach has side obstacles and flank passages, never a solid road plug.
        for(int32 J=0;J<3;++J)
        {
            const float Offset=(J%2 ? -1.f:1.f)*(1100+I*80);
            const FVector P=Local(FVector(Offset,4400+J*1250,150+I*20));
            Shape(W,FString::Printf(TEXT("%s_Ridge_%d"),*Prefix,J),P,FVector(700+I*120,950,300+I*40),Stone,true,Yaws[I]+J*13.f,true);
            Shape(W,FString::Printf(TEXT("%s_Cover_%d"),*Prefix,J),Local(FVector(-Offset*.6f,4900+J*1050,70)),FVector(280,180,140),Wood,true,Yaws[I]);
        }
        Shape(W,Prefix+TEXT("_ArenaGround"),Local(FVector(0,7000,5)),FVector(2100,1700,6),Road,false,Yaws[I]);
    }
    // Abandoned east-side shelter and west-side broken camp distinguish excursions.
    DesignBuilding(W,TEXT("ABANDONED SHED"),FVector(6200,1700,0),90,FVector(900,800,0),Wood);
    Shape(W,TEXT("West_RuinedCamp_Wall"),FVector(-6500,-1700,150),FVector(1300,80,300),Stone);
    Shape(W,TEXT("West_RuinedCamp_Cover"),FVector(-6100,-1500,80),FVector(300,220,160),Wood);
    Shape(W,TEXT("MineEntrance_Left"),FVector(-500,-8500,350),FVector(220,300,700),Stone);
    Shape(W,TEXT("MineEntrance_Right"),FVector(500,-8500,350),FVector(220,300,700),Stone);
    Shape(W,TEXT("MineEntrance_Lintel"),FVector(0,-8500,720),FVector(1220,300,180),Wood);
    Label(W,TEXT("MineEntrance_Label"),TEXT("MINE ENTRANCE"),FVector(0,-8310,710),90,95);
    Marker(W,TEXT("Mine_LevelTransitionPoint"),FVector(0,-8500,100),TEXT("MineEntrance"),TEXT("South"));
    Goals.Add(FVector(0,-8100,0));
    for(int32 Side:{-1,1})
    {
        Shape(W,FString::Printf(TEXT("IslandBoundary_X_%d"),Side),FVector(Side*10050,0,350),FVector(100,20200,700),Stone);
        Shape(W,FString::Printf(TEXT("IslandBoundary_Y_%d"),Side),FVector(0,Side*10050,350),FVector(20000,100,700),Stone);
    }
    DesignGameMode(W,FVector(-650,-1700,110),FRotator(0,70,0));
    DesignLighting(W);
    for(FLevelEditorViewportClient* View:GEditor->GetLevelViewportClients()) if(View && View->IsPerspective())
    { View->SetViewLocation(FVector(12500,-12500,12500)); View->SetViewRotation(FRotator(-35.264f,135,0)); }
    return BuildSaveDesign(W,Path,FVector(0,0,700),FVector(20000,20000,3000),FVector(-650,-1700,0),Goals);
}

static bool CreateMine()
{
    const TCHAR* Path=TEXT("/Game/Prototype/Maps/Lvl_DesignMine");
    if(FPackageName::DoesPackageExist(Path)) { UE_LOG(LogTemp,Error,TEXT("DESIGN_MAP exists, not overwriting %s"),Path); return false; }
    UWorld* W=UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UMaterialInterface* Stone=Material(TEXT("MI_BlockoutStone"),FLinearColor(.24f,.25f,.26f));
    UMaterialInterface* Wood=Material(TEXT("MI_BlockoutWood"),FLinearColor(.22f,.11f,.055f));
    UMaterialInterface* Ore=Material(TEXT("MI_BlockoutOre"),FLinearColor(.12f,.4f,.48f));
    Shape(W,TEXT("Mine_EntranceFloor"),FVector(0,0,-50),FVector(1800,1800,100),Stone);
    Shape(W,TEXT("Mine_UpperFloor"),FVector(3500,0,-850),FVector(1800,2000,100),Stone);
    Shape(W,TEXT("Mine_LowerFloor"),FVector(8100,0,-2450),FVector(2000,2400,100),Stone);
    auto Ramp=[&](const FString& Name,FVector A,FVector B)
    {
        const FVector Delta=B-A;
        const float Length=Delta.Size();
        AStaticMeshActor* Floor=Shape(W,Name,(A+B)*.5f-FVector(0,0,50),FVector(Length,650,100),Wood);
        Floor->SetMobility(EComponentMobility::Movable);
        Floor->SetActorRotation(Delta.Rotation());
        Floor->SetMobility(EComponentMobility::Static);
        for(int32 Side:{-1,1})
        {
            AStaticMeshActor* Rail=Shape(W,Name+FString::Printf(TEXT("_Rail_%d"),Side),(A+B)*.5f+FVector(0,Side*350,100),FVector(Length,70,180),Wood);
            Rail->SetMobility(EComponentMobility::Movable); Rail->SetActorRotation(Delta.Rotation()); Rail->SetMobility(EComponentMobility::Static);
        }
    };
    Ramp(TEXT("Mine_Descent_EntranceToUpper"),FVector(880,0,0),FVector(2620,0,-800));
    Ramp(TEXT("Mine_Descent_UpperToLower"),FVector(4380,0,-800),FVector(7120,0,-2400));
    const FVector Centers[]={FVector(0,0,0),FVector(3500,0,-800),FVector(8100,0,-2400)};
    const float Widths[]={1800.f,2000.f,2400.f};
    const TCHAR* Titles[]={TEXT("MINE ENTRANCE"),TEXT("UPPER LEVEL"),TEXT("LOWER LEVEL")};
    for(int32 I=0;I<3;++I)
    {
        const FVector P=Centers[I];
        for(int32 Side:{-1,1})
            Shape(W,FString::Printf(TEXT("Mine_%d_SideWall_%d"),I,Side),P+FVector(0,Side*Widths[I]*.5f,250),FVector(I==2?2000:1800,100,500),Stone);
        for(int32 End:{-1,1}) for(int32 Side:{-1,1})
            Shape(W,FString::Printf(TEXT("Mine_%d_EndGuard_%d_%d"),I,End,Side),
                P+FVector(End*(I==2?1000.f:900.f),Side*(Widths[I]*.5f+400.f)*.5f,250),
                FVector(100,Widths[I]*.5f-400.f,500),Stone);
        Label(W,FString::Printf(TEXT("Mine_%d_Sign"),I),Titles[I],P+FVector(0,0,450),180,85);
        for(int32 J=0;J<3;++J)
        {
            const FVector Q=P+FVector(-350+J*350,Widths[I]*.5f-180,120);
            Shape(W,FString::Printf(TEXT("Mine_%d_Ore_%d"),I,J),Q,FVector(140,140,240),Ore,true,0,true);
            Marker(W,FString::Printf(TEXT("Mine_%d_ResourcePoint_%d"),I,J),Q+FVector(0,-150,0),TEXT("MiningResource"),FName(Titles[I]));
        }
        APointLight* Light=W->SpawnActor<APointLight>(P+FVector(0,0,380),FRotator::ZeroRotator);
        Light->PointLightComponent->SetMobility(EComponentMobility::Movable);
        Light->PointLightComponent->SetIntensity(18000.f);
        Light->PointLightComponent->SetAttenuationRadius(3500.f);
        Light->PointLightComponent->SetLightColor(FLinearColor(1.f,.72f,.4f));
    }
    Shape(W,TEXT("Mine_Entrance_Backstop"),FVector(-950,0,250),FVector(100,1900,500),Stone);
    Shape(W,TEXT("Mine_Lower_Backstop"),FVector(9150,0,-2150),FVector(100,2500,500),Stone);
    Marker(W,TEXT("Mine_ExitToVillage"),FVector(-650,0,100),TEXT("MineExit"),TEXT("Entrance"));
    Marker(W,TEXT("Mine_RepairPartPoint"),FVector(8550,-650,-2300),TEXT("QuestEvent"),TEXT("Lower"));
    DesignGameMode(W,FVector(-500,0,110),FRotator::ZeroRotator);
    return BuildSaveDesign(W,Path,FVector(4100,0,-1100),FVector(11500,5000,6000),FVector(-500,0,0),{FVector(3500,0,-800),FVector(8100,0,-2400)});
}

static FAutoConsoleCommand DesignCommand(TEXT("PrototypeMap.CreateDesign"),TEXT("Create separate village/outskirts and descending mine blockouts from the design brief."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            const bool Village=CreateVillage();
            const bool Mine=CreateMine();
            UE_LOG(LogTemp,Display,TEXT("DESIGN_MAP COMPLETE village=%d mine=%d"),Village,Mine);
            if(FParse::Param(FCommandLine::Get(),TEXT("PrototypeMapBatch"))) FPlatformMisc::RequestExit(false);
            return false;
        }),2.f);
    }));

// Latest supplied deck: fixed five lairs, mine entrance INSIDE the village,
// two initial passages, damaged wall and a future breacher shortcut (slides 48,52-62).
// No mine interior or new mine map is generated by this command.
static void CreateLatestVillage(bool bApprovedLayout=false)
{
    const TCHAR* Path=bApprovedLayout ? TEXT("/Game/Prototype/Maps/Lvl_ApprovedLayout") : TEXT("/Game/Prototype/Maps/Lvl_LatestVillage");
    if(!GEditor || GEditor->PlayWorld) return;
    if(FPackageName::DoesPackageExist(Path))
    { UE_LOG(LogTemp,Error,TEXT("LATEST_MAP exists; not overwriting %s"),Path); return; }
    UWorld* W=UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UMaterialInterface* Sand=Material(TEXT("MI_BlockoutSand"),FLinearColor(.43f,.31f,.19f));
    UMaterialInterface* Road=Material(TEXT("MI_BlockoutRoad"),FLinearColor(.22f,.17f,.115f));
    UMaterialInterface* Stone=Material(TEXT("MI_BlockoutStone"),FLinearColor(.24f,.25f,.26f));
    UMaterialInterface* Wood=Material(TEXT("MI_BlockoutWood"),FLinearColor(.22f,.11f,.055f));
    UMaterialInterface* Blue=Material(TEXT("MI_BlockoutAircraft"),FLinearColor(.07f,.25f,.34f));
    UMaterialInterface* LairMat=Material(TEXT("MI_BlockoutColony"),FLinearColor(.62f,.16f,.04f));

    Shape(W,TEXT("OpenField_Ground_200m"),FVector(0,0,-50),FVector(20000,20000,100),Sand);
    Shape(W,TEXT("Village_SupportSpace"),FVector(0,0,3),FVector(6100,6100,6),Road,false);
    Shape(W,TEXT("Village_MainApproach"),FVector(0,-4800,4),FVector(1000,3900,6),Road,false);
    Shape(W,TEXT("Village_SecondaryApproach"),FVector(4750,600,4),FVector(3300,650,6),Road,false);
    Shape(W,TEXT("Village_CentralDefensePlaza"),FVector(-300,650,6),FVector(2700,2400,6),Stone,false);

    DesignBuilding(W,TEXT("SALOON"),FVector(-1950,-100,0),-90,FVector(900,1000,0),Wood);
    DesignBuilding(W,TEXT("CLINIC"),FVector(1900,1800,0),90,FVector(1000,900,0),Wood);
    DesignBuilding(W,TEXT("WORKSHOP"),FVector(-1900,-2150,0),-90,FVector(1000,1000,0),Wood);
    DesignBuilding(W,TEXT("EXCHANGE"),FVector(1950,-1700,0),90,FVector(1100,1000,0),Wood);
    Marker(W,TEXT("RoleStart_Sheriff"),FVector(-1950,-100,110),TEXT("RoleSpawn"),TEXT("Sheriff"));
    Marker(W,TEXT("RoleStart_Doctor"),FVector(1900,1800,110),TEXT("RoleSpawn"),TEXT("Doctor"));
    Marker(W,TEXT("RoleStart_Mechanic"),FVector(-1900,-2150,110),TEXT("RoleSpawn"),TEXT("Mechanic"));

    // Recovery/repair objective, separate from the mine that will be the loss target.
    const FVector Plane(0,-1250,0);
    Shape(W,TEXT("Aircraft_RepairArea"),Plane+FVector(0,0,8),FVector(2200,1700,8),Stone,false);
    Shape(W,TEXT("Aircraft_Wreck_Body"),Plane+FVector(0,0,125),FVector(900,210,230),Blue);
    Shape(W,TEXT("Aircraft_Wreck_Wings"),Plane+FVector(-80,0,120),FVector(220,1100,40),Blue);
    Shape(W,TEXT("Aircraft_Wreck_Tail"),Plane+FVector(-390,0,280),FVector(130,40,280),Blue);
    Label(W,TEXT("Aircraft_RepairSign"),TEXT("AIRCRAFT REPAIR"),Plane+FVector(0,-210,400),-90,80);
    Marker(W,TEXT("Aircraft_FinalRepairPoint"),Plane+FVector(550,0,100),TEXT("FinalRepair"),TEXT("Village"));
    Marker(W,TEXT("Aircraft_EscortHoldPoint"),Plane+FVector(0,-650,100),TEXT("EscortDefense"),TEXT("Village"));

    // The mine is an entrance only, within the northwest part of the stone wall.
    const FVector Mine(-1900,2200,0);
    Shape(W,TEXT("MineEntrance_LeftPier"),Mine+FVector(-470,0,300),FVector(200,500,600),Stone);
    Shape(W,TEXT("MineEntrance_RightPier"),Mine+FVector(470,0,300),FVector(200,500,600),Stone);
    Shape(W,TEXT("MineEntrance_Lintel"),Mine+FVector(0,0,630),FVector(1140,500,160),Wood);
    Shape(W,TEXT("MineEntrance_ClosedInterior"),Mine+FVector(0,260,250),FVector(750,60,500),Stone);
    Shape(W,TEXT("MineEntrance_DefenseApron"),Mine+FVector(0,-550,5),FVector(1700,1200,6),Stone,false);
    Label(W,TEXT("MineEntrance_DefenseSign"),TEXT("MINE"),Mine+FVector(0,-290,630),-90,120);
    Marker(W,TEXT("Mine_PrimaryDefenseTarget"),Mine+FVector(0,-350,110),TEXT("PrimaryDefenseTarget"),TEXT("Mine"));
    Marker(W,TEXT("Mine_Entrance_Reserved"),Mine+FVector(0,80,110),TEXT("MineEntrance"),TEXT("Village"));

    // Stone perimeter. Only south main gate and east rubble passage are initially open.
    Shape(W,TEXT("StoneWall_North"),FVector(0,3350,210),FVector(6900,180,420),Stone);
    for(int32 Side:{-1,1})
        Shape(W,FString::Printf(TEXT("StoneWall_South_%d"),Side),FVector(Side*1925,-3350,210),FVector(2950,180,420),Stone);
    Shape(W,TEXT("StoneWall_East_BelowPassage"),FVector(3350,-1525,210),FVector(180,3650,420),Stone);
    Shape(W,TEXT("StoneWall_East_AbovePassage"),FVector(3350,2125,210),FVector(180,2450,420),Stone);
    Shape(W,TEXT("StoneWall_West_BelowBreach"),FVector(-3350,-1225,210),FVector(180,4250,420),Stone);
    Shape(W,TEXT("StoneWall_West_AboveBreach"),FVector(-3350,2425,210),FVector(180,1850,420),Stone);
    AStaticMeshActor* Breach=Shape(W,TEXT("StoneWall_BreacherShortcut_Closed"),FVector(-3350,1200,210),FVector(180,600,420),Stone);
    Breach->Tags={TEXT("FutureBreakableWall"),TEXT("MineShortcut")};
    Marker(W,TEXT("Breacher_OutsideAttackAnchor"),FVector(-3800,1200,100),TEXT("WallAttackAnchor"),TEXT("BreachShortcut"));
    Marker(W,TEXT("Breacher_InsideExitAnchor"),FVector(-2900,1200,100),TEXT("BreachExit"),TEXT("BreachShortcut"));
    Marker(W,TEXT("MainGate_DefenseAnchor"),FVector(0,-3000,100),TEXT("VillageEntrance"),TEXT("Main"));
    Marker(W,TEXT("SecondaryPassage_DefenseAnchor"),FVector(3000,600,100),TEXT("VillageEntrance"),TEXT("Secondary"));
    // Crash debris lines the openings without closing their clear walking corridors.
    for(int32 Side:{-1,1})
    {
        Shape(W,FString::Printf(TEXT("CrashRubble_Main_%d"),Side),FVector(Side*620,-3420,75),FVector(260,400,150),Stone,true,Side*18.f);
        Shape(W,FString::Printf(TEXT("CrashRubble_Secondary_%d"),Side),FVector(3400,600+Side*430,85),FVector(350,190,170),Stone,true,Side*22.f);
    }
    Label(W,TEXT("MainGate_Sign"),TEXT("MAIN GATE"),FVector(0,-3260,440),-90,85);
    Label(W,TEXT("SecondaryPassage_Sign"),TEXT("RUBBLE PASSAGE"),FVector(3250,600,440),0,65);

    // Two future defensive installations, outside the main shared circulation space.
    for(int32 I=0;I<2;++I)
    {
        const FVector P=I==0?FVector(2700,2600,0):FVector(-2650,-2750,0);
        Shape(W,FString::Printf(TEXT("AirDefense_%d_Foundation"),I),P+FVector(0,0,40),FVector(420,420,80),Stone);
        Shape(W,FString::Printf(TEXT("AirDefense_%d_Column"),I),P+FVector(0,0,260),FVector(90,90,440),Wood);
        Shape(W,FString::Printf(TEXT("AirDefense_%d_Platform"),I),P+FVector(0,0,490),FVector(500,500,40),Wood);
        Marker(W,FString::Printf(TEXT("AirDefense_%d_BuildPoint"),I),P+FVector(0,0,550),TEXT("DefenseTower"),TEXT("AntiAir"));
    }

    // Five fixed sites in one continuous field. Number is identity, not difficulty.
    const FVector Lairs[]={FVector(-6600,6500,0),FVector(2300,7700,0),FVector(7600,1900,0),FVector(5700,-6500,0),FVector(-6600,-5100,0)};
    TArray<FVector> Goals;
    for(int32 I=0;I<5;++I)
    {
        const FVector P=Lairs[I];
        const FRotator Facing=(-P).Rotation();
        const FRotator R(0,Facing.Yaw-90.f,0); // local +Y faces the village
        auto Local=[&](FVector V){return P+R.RotateVector(V);};
        const FString Name=FString::Printf(TEXT("Lair_%02d"),I+1);
        Shape(W,Name+TEXT("_ApproachClearing"),P+FVector(0,0,5),FVector(2100,2200,6),Road,false,R.Yaw);
        Shape(W,Name+TEXT("_LeftRock"),Local(FVector(-550,0,290)),FVector(400,850,580),Stone,true,R.Yaw,true);
        Shape(W,Name+TEXT("_RightRock"),Local(FVector(550,0,290)),FVector(400,850,580),Stone,true,R.Yaw,true);
        Shape(W,Name+TEXT("_EntranceArch"),Local(FVector(0,-50,650)),FVector(1500,750,240),Stone,true,R.Yaw);
        Shape(W,Name+TEXT("_CoreMarker"),Local(FVector(0,-280,200)),FVector(250,250,400),LairMat,true,R.Yaw);
        Label(W,Name+TEXT("_EntranceSign"),FString::Printf(TEXT("LAIR %d"),I+1),Local(FVector(0,420,720)),Facing.Yaw,85);
        Marker(W,Name+TEXT("_FixedSpawnPoint"),Local(FVector(0,800,100)),TEXT("FixedLairSpawn"),FName(*Name));
        Marker(W,Name+TEXT("_PartyAssembly"),Local(FVector(0,650,100)),TEXT("PartyAssembly"),FName(*Name));
        Marker(W,Name+TEXT("_DungeonEntrance"),Local(FVector(0,250,100)),TEXT("LairDungeonEntrance"),FName(*Name));
        Marker(W,Name+TEXT("_BossReserved"),Local(FVector(0,-700,100)),TEXT("LairBoss"),FName(*Name));
        Marker(W,Name+TEXT("_RewardReserved"),Local(FVector(-450,-650,100)),TEXT("MainQuestReward"),FName(*Name));
        Goals.Add(Local(FVector(0,700,0)));
    }
    // Sparse field landmarks leave freedom to discover the five sites without a minimap.
    const FVector Rocks[]={FVector(-4700,4200,0),FVector(-3800,6200,0),FVector(800,5200,0),FVector(5000,4300,0),
        FVector(6200,-1300,0),FVector(4000,-4500,0),FVector(-3000,-5900,0),FVector(-7300,-1900,0)};
    for(int32 I=0;I<8;++I)
    {
        Shape(W,FString::Printf(TEXT("FieldLandmark_Rock_%02d"),I),Rocks[I]+FVector(0,0,200),FVector(650,950,400),Stone,true,I*27.f,true);
        Shape(W,FString::Printf(TEXT("FieldCover_%02d"),I),Rocks[I]+FVector(600,350,65),FVector(260,180,130),Wood,true,I*17.f);
    }
    Marker(W,TEXT("Field_RecoveryPart_01"),FVector(-5100,2200,100),TEXT("FieldRepairPart"),TEXT("MainQuest"));
    Marker(W,TEXT("Field_RecoveryPart_02"),FVector(4800,-3600,100),TEXT("FieldRepairPart"),TEXT("MainQuest"));
    for(int32 Side:{-1,1})
    {
        Shape(W,FString::Printf(TEXT("FieldBoundary_X_%d"),Side),FVector(Side*10050,0,350),FVector(100,20200,700),Stone);
        Shape(W,FString::Printf(TEXT("FieldBoundary_Y_%d"),Side),FVector(0,Side*10050,350),FVector(20000,100,700),Stone);
    }
    DesignGameMode(W,FVector(-1950,-100,110),FRotator::ZeroRotator);
    DesignLighting(W);
    Goals.Add(Mine+FVector(0,-500,0));
    Goals.Add(FVector(0,-3800,0));
    Goals.Add(FVector(3800,600,0));
    FVector StartPoint(-1950,-100,0);
    if(bApprovedLayout)
    {
        // Apply the approved drawing's topology, not a central walled fortress.
        // Town abuts ONE rear wall; the open field continues to both sides/front.
        TArray<AActor*> Actors;
        for(TActorIterator<AActor> It(W);It;++It) Actors.Add(*It);
        auto MoveEditorActor=[](AActor* Actor,const FVector& Position,float YawDelta=0.f)
        {
            UStaticMeshComponent* Mesh=Actor->FindComponentByClass<UStaticMeshComponent>();
            const bool WasStatic=Mesh && Mesh->Mobility==EComponentMobility::Static;
            if(WasStatic) Mesh->SetMobility(EComponentMobility::Movable);
            Actor->SetActorLocation(Position);
            Actor->SetActorRotation(Actor->GetActorRotation()+FRotator(0,YawDelta,0));
            if(WasStatic) Mesh->SetMobility(EComponentMobility::Static);
            Actor->PostEditMove(true);
        };
        const FVector ApprovedLairs[]={FVector(-7200,5100,0),FVector(7200,5100,0),
            FVector(-7200,-5200,0),FVector(0,-8200,0),FVector(7200,-5200,0)};
        const FVector TownCenter(0,5300,0);
        for(AActor* Actor:Actors)
        {
            const FString Name=Actor->GetActorLabel();
            if(Name.StartsWith(TEXT("StoneWall_")) || Name.StartsWith(TEXT("Breacher_"))
                || Name.StartsWith(TEXT("CrashRubble_")) || Name.StartsWith(TEXT("MainGate_"))
                || Name.StartsWith(TEXT("SecondaryPassage_")) || Name.StartsWith(TEXT("FieldBoundary_"))
                || Name.StartsWith(TEXT("FieldLandmark_")) || Name.StartsWith(TEXT("FieldCover_"))
                || Name==TEXT("Village_MainApproach") || Name==TEXT("Village_SecondaryApproach"))
            { Actor->Destroy(); continue; }
            bool IsLair=false;
            for(int32 I=0;I<5;++I)
                if(Name.StartsWith(FString::Printf(TEXT("Lair_%02d_"),I+1)))
                {
                    const float OldYaw=(-Lairs[I]).Rotation().Yaw-90.f;
                    const float NewYaw=(TownCenter-ApprovedLairs[I]).Rotation().Yaw-90.f;
                    const float DeltaYaw=NewYaw-OldYaw;
                    MoveEditorActor(Actor,ApprovedLairs[I]+FRotator(0,DeltaYaw,0).RotateVector(Actor->GetActorLocation()-Lairs[I]),DeltaYaw);
                    IsLair=true; break;
                }
            if(IsLair) continue;
            FVector Shift=FVector::ZeroVector;
            if(Name.StartsWith(TEXT("SALOON")) || Name==TEXT("RoleStart_Sheriff") || Name==TEXT("PlayerStart_DesignMap")) Shift=FVector(650,6600,0);
            else if(Name.StartsWith(TEXT("CLINIC")) || Name==TEXT("RoleStart_Doctor")) Shift=FVector(-600,4700,0);
            else if(Name.StartsWith(TEXT("WORKSHOP")) || Name==TEXT("RoleStart_Mechanic")) Shift=FVector(600,6550,0);
            else if(Name.StartsWith(TEXT("EXCHANGE"))) Shift=FVector(-650,6100,0);
            else if(Name.StartsWith(TEXT("Mine"))) Shift=FVector(1900,5800,0);
            else if(Name.StartsWith(TEXT("Aircraft"))) Shift=FVector(0,4450,0);
            else if(Name.StartsWith(TEXT("AirDefense"))) Shift=FVector(0,5000,0);
            else if(Name.StartsWith(TEXT("Village_"))) Shift=FVector(0,5300,0);
            if(!Shift.IsNearlyZero()) MoveEditorActor(Actor,Actor->GetActorLocation()+Shift);
        }

        Shape(W,TEXT("RearStoneWall_Continuous"),FVector(0,8500,260),FVector(20000,180,520),Stone);
        for(int32 I=0;I<7;++I)
            Shape(W,FString::Printf(TEXT("RearCliff_%02d"),I),FVector(-9000+I*3000,9500,650),FVector(2600,1500,1300),Stone,true,I*11.f,true);

        // Two marked front approaches among short damaged barrier fragments.
        // No connecting side walls or rectangular stone perimeter are created.
        Shape(W,TEXT("FrontFragment_Left"),FVector(-2500,2000,100),FVector(1000,180,200),Stone);
        AStaticMeshActor* FrontBreach=Shape(W,TEXT("FrontFragment_FutureBreach"),FVector(-500,2000,100),FVector(1000,180,200),Stone);
        FrontBreach->Tags={TEXT("FutureBreakableWall"),TEXT("MineShortcut")};
        Shape(W,TEXT("FrontFragment_CenterRight"),FVector(575,2000,95),FVector(1050,180,190),Stone);
        Shape(W,TEXT("FrontFragment_Right"),FVector(2450,2000,90),FVector(1100,180,180),Stone);
        for(int32 Side:{-1,1})
        {
            Shape(W,FString::Printf(TEXT("FrontRubble_Main_%d"),Side),FVector(-1500+Side*620,1850,55),FVector(170,220,110),Stone,true,Side*16.f);
            Shape(W,FString::Printf(TEXT("FrontRubble_Secondary_%d"),Side),FVector(1500+Side*500,1870,45),FVector(160,230,90),Stone,true,Side*19.f);
            // Short wooden posts suggest the village edge without closing it.
            for(int32 I=0;I<3;++I)
                Shape(W,FString::Printf(TEXT("VillageEdgePost_%d_%d"),Side,I),FVector(Side*2850,4000+I*1500,75),FVector(50,50,150),Wood);
        }
        Marker(W,TEXT("MainApproach_DefenseAnchor"),FVector(-1500,2200,100),TEXT("VillageEntrance"),TEXT("Main"));
        Marker(W,TEXT("SecondaryApproach_DefenseAnchor"),FVector(1500,2200,100),TEXT("VillageEntrance"),TEXT("Secondary"));
        Marker(W,TEXT("FrontBreach_OutsideAnchor"),FVector(-500,1600,100),TEXT("WallAttackAnchor"),TEXT("FrontShortcut"));
        Label(W,TEXT("MainApproach_Sign"),TEXT("MAIN APPROACH"),FVector(-1500,2250,300),-90,70);
        Label(W,TEXT("SecondaryApproach_Sign"),TEXT("SIDE APPROACH"),FVector(1500,2250,300),-90,65);

        auto Trail=[&](const FString& Name,FVector A,FVector B,float Width=450.f)
        {
            A.Z=B.Z=5.f;
            Shape(W,Name,(A+B)*.5f,FVector(FVector::Dist2D(A,B),Width,5),Road,false,(B-A).Rotation().Yaw);
        };
        Trail(TEXT("Trail_Main_01"),FVector(-1500,2200,0),FVector(-3000,400,0),600);
        Trail(TEXT("Trail_Main_02"),FVector(-3000,400,0),FVector(-4500,-1500,0));
        Trail(TEXT("Trail_LeftUpper"),FVector(-3000,400,0),FVector(-6500,4900,0));
        Trail(TEXT("Trail_LeftLower"),FVector(-4500,-1500,0),FVector(-6600,-4700,0));
        Trail(TEXT("Trail_Secondary_01"),FVector(1500,2200,0),FVector(3300,400,0),500);
        Trail(TEXT("Trail_Secondary_02"),FVector(3300,400,0),FVector(4500,-1500,0));
        Trail(TEXT("Trail_RightUpper"),FVector(3300,400,0),FVector(6500,4900,0));
        Trail(TEXT("Trail_RightLower"),FVector(4500,-1500,0),FVector(6600,-4700,0));
        Trail(TEXT("Trail_FieldLoop_Left"),FVector(-4500,-1500,0),FVector(0,-3400,0));
        Trail(TEXT("Trail_FieldLoop_Right"),FVector(0,-3400,0),FVector(4500,-1500,0));
        Trail(TEXT("Trail_LowerLoop_Left"),FVector(-6600,-4700,0),FVector(0,-6100,0));
        Trail(TEXT("Trail_LowerLoop_Right"),FVector(0,-6100,0),FVector(6600,-4700,0));
        Trail(TEXT("Trail_CenterConnection"),FVector(0,-3400,0),FVector(0,-6100,0));
        Trail(TEXT("Trail_BottomLair"),FVector(0,-6100,0),FVector(0,-7500,0));

        UMaterialInterface* Green=Material(TEXT("MI_BlockoutGreen"),FLinearColor(.17f,.24f,.105f));
        const FVector Landmarks[]={FVector(-4400,4500,0),FVector(4400,4500,0),FVector(-6000,2300,0),FVector(6000,2300,0),
            FVector(-4400,-500,0),FVector(4400,-500,0),FVector(-4000,-4400,0),FVector(4000,-4400,0),FVector(-1400,-6500,0),FVector(1600,-6500,0)};
        for(int32 I=0;I<10;++I)
        {
            Shape(W,FString::Printf(TEXT("FieldRock_%02d"),I),Landmarks[I]+FVector(0,0,170),FVector(700,1000,340),Stone,true,I*21.f,true);
            Shape(W,FString::Printf(TEXT("FieldLowCover_%02d"),I),Landmarks[I]+FVector(550,350,55),FVector(220,200,110),Wood,true,I*19.f);
            for(int32 J=0;J<2;++J)
            {
                const FVector P=Landmarks[I]+FVector(-550-J*260,400+J*300,0);
                Shape(W,FString::Printf(TEXT("TreeTrunk_%02d_%d"),I,J),P+FVector(0,0,110),FVector(55,55,220),Wood,true,0,true);
                AStaticMeshActor* Canopy=Shape(W,FString::Printf(TEXT("TreeCanopy_%02d_%d"),I,J),P+FVector(0,0,340),FVector(300,300,440),Green,false);
                Canopy->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone")));
            }
        }
        // Invisible limits keep this blockout playable without a visible outer fortress.
        const FVector LimitCenters[]={FVector(-10050,0,500),FVector(10050,0,500),FVector(0,-10050,500)};
        const FVector LimitSizes[]={FVector(100,20000,1000),FVector(100,20000,1000),FVector(20200,100,1000)};
        for(int32 I=0;I<3;++I)
        {
            ABlockingVolume* Limit=W->SpawnActor<ABlockingVolume>();
            Limit->SetActorLabel(FString::Printf(TEXT("InvisibleMapLimit_%d"),I));
            UCubeBuilder* Builder=NewObject<UCubeBuilder>(Limit);
            Builder->X=LimitSizes[I].X; Builder->Y=LimitSizes[I].Y; Builder->Z=LimitSizes[I].Z;
            UActorFactory::CreateBrushForVolumeActor(Limit,Builder);
            Limit->SetActorLocation(LimitCenters[I]); Limit->PostEditChange();
        }
        Goals.Reset();
        for(const FVector& P:ApprovedLairs) Goals.Add(P+(TownCenter-P).GetSafeNormal2D()*700.f);
        Goals.Add(FVector(0,7500,0));
        Goals.Add(FVector(-1500,1600,0)); Goals.Add(FVector(1500,1600,0));
        StartPoint=FVector(-1300,6500,0);
        UE_LOG(LogTemp,Display,TEXT("APPROVED_LAYOUT town_center=(0,5300) mine=(0,8000) rear_wall_y=8500 lairs=5 old_perimeter_removed=1 mine_interior_created=0"));
    }
    for(FLevelEditorViewportClient* View:GEditor->GetLevelViewportClients()) if(View && View->IsPerspective())
    {
        View->SetViewLocation(bApprovedLayout ? FVector(0,-14500,19000) : FVector(12000,-13500,13000));
        View->SetViewRotation(bApprovedLayout ? FRotator(-52.65f,90.f,0) : FRotator(-35.264f,132.f,0));
    }
    const bool OK=BuildSaveDesign(W,Path,FVector(0,0,900),FVector(20000,20000,3600),StartPoint,Goals);
    UE_LOG(LogTemp,Display,TEXT("LATEST_MAP COMPLETE success=%d fixed_lairs=5 initial_entrances=2 mine_interior_created=0"),OK);
    if(FParse::Param(FCommandLine::Get(),TEXT("PrototypeMapBatch"))) FPlatformMisc::RequestExit(false);
}
static FAutoConsoleCommand LatestCommand(TEXT("PrototypeMap.CreateLatest"),TEXT("Create the latest brief village/field map with five fixed lairs; no mine interior."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){ CreateLatestVillage(); return false; }),2.f);
    }));
static FAutoConsoleCommand InspectApprovedCommand(TEXT("PrototypeMap.InspectApproved"),TEXT("Inspect saved approved map actor placement."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            UWorld* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Prototype/Maps/Lvl_ApprovedLayout"));
            if(W) for(TActorIterator<AActor> It(W);It;++It)
            {
                FVector Center,Extent; It->GetActorBounds(false,Center,Extent);
                UE_LOG(LogTemp,Display,TEXT("MAP_ACTOR %s pos=%s center=%s extent=%s"),*It->GetActorLabel(),*It->GetActorLocation().ToString(),*Center.ToString(),*Extent.ToString());
            }
            FPlatformMisc::RequestExit(false); return false;
        }),2.f);
    }));
static void MoveCrashToSide()
{
    const TCHAR* Path=TEXT("/Game/Prototype/Maps/Lvl_ApprovedLayout");
    UWorld* W=UEditorLoadingAndSavingUtils::LoadMap(Path);
    if(!W) return;
    TArray<AActor*> Actors;
    for(TActorIterator<AActor> It(W);It;++It) Actors.Add(*It);
    UMaterialInterface* Stone=Material(TEXT("MI_BlockoutStone"),FLinearColor(.3f,.32f,.34f));
    UMaterialInterface* Road=Material(TEXT("MI_BlockoutRoad"),FLinearColor(.25f,.18f,.11f));
    for(AActor* Actor:Actors)
    {
        const FString Name=Actor->GetActorLabel();
        if(Name==TEXT("SideCrash_Completed"))
        { if(FParse::Param(FCommandLine::Get(),TEXT("PrototypeMapBatch"))) FPlatformMisc::RequestExit(false); return; }
    }
    for(AActor* Actor:Actors)
    {
        const FString Name=Actor->GetActorLabel();
        if(Name==TEXT("FrontFragment_Left2"))
        {
            FVector Center,Extent; Actor->GetActorBounds(false,Center,Extent);
            UStaticMeshComponent* Mesh=Actor->FindComponentByClass<UStaticMeshComponent>();
            UMaterialInterface* WallMaterial=Mesh ? Mesh->GetMaterial(0) : Stone;
            const float Bottom=Center.Y-Extent.Y,Top=Center.Y+Extent.Y;
            Shape(W,TEXT("SideCrash_BarrierSouth"),FVector(Center.X,(Bottom+2700.f)*.5f,Center.Z),FVector(Extent.X*2,2700.f-Bottom,Extent.Z*2),WallMaterial);
            Shape(W,TEXT("SideCrash_BarrierNorth"),FVector(Center.X,(3700.f+Top)*.5f,Center.Z),FVector(Extent.X*2,Top-3700.f,Extent.Z*2),WallMaterial);
            Actor->Destroy(); continue;
        }
        if(Name.StartsWith(TEXT("SecondaryApproach_"))
            || Name==TEXT("VillageEdgePost_1_0") || Name==TEXT("Trail_Secondary_01")
            || Actor->IsA<ANavMeshBoundsVolume>())
        { Actor->Destroy(); continue; }
        if(Name.StartsWith(TEXT("MainApproach_")))
        {
            Actor->SetActorLocation(Actor->GetActorLocation()+FVector(3000,0,0));
            Actor->PostEditMove(true);
        }
        if(Name.StartsWith(TEXT("Aircraft")))
        {
            // Nose points west: the aircraft entered from the eastern field.
            const FVector Relative=Actor->GetActorLocation()-FVector(0,3200,0);
            UStaticMeshComponent* Mesh=Actor->FindComponentByClass<UStaticMeshComponent>();
            if(Mesh) Mesh->SetMobility(EComponentMobility::Movable);
            Actor->SetActorLocation(FVector(1800,3200,0)+FRotator(0,180,0).RotateVector(Relative));
            Actor->SetActorRotation(Actor->GetActorRotation()+FRotator(0,180,0));
            if(Mesh) Mesh->SetMobility(EComponentMobility::Static);
            Actor->PostEditMove(true);
        }
    }
    // Preserve the user's front gate and the rest of the side walls.
    Shape(W,TEXT("SideCrash_RubbleSouth"),FVector(2940,2640,60),FVector(320,160,120),Stone,true,24);
    Shape(W,TEXT("SideCrash_RubbleNorth"),FVector(2780,3750,75),FVector(300,180,150),Stone,true,-28);
    Shape(W,TEXT("SideCrash_SkidMark"),FVector(3000,3200,7),FVector(1900,420,6),Road,false);
    const FVector A(3300,400,5),B(3400,3200,5);
    Shape(W,TEXT("Trail_SideCrashConnection"),(A+B)*.5f,FVector(FVector::Dist2D(A,B),500,5),Road,false,(B-A).Rotation().Yaw);
    Marker(W,TEXT("SecondaryApproach_DefenseAnchor"),FVector(2750,3200,100),TEXT("VillageEntrance"),TEXT("Secondary"));
    Label(W,TEXT("SecondaryApproach_Sign"),TEXT("CRASH BREACH"),FVector(3100,3200,320),0,65);
    Marker(W,TEXT("SideCrash_Completed"),FVector(2850,3200,100),TEXT("CrashBreach"),TEXT("Village"));
    const bool Blocked=W->SweepTestByChannel(FVector(2400,3200,110),FVector(3400,3200,110),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(42,96));
    UE_LOG(LogTemp,Display,TEXT("SIDE_CRASH direct_capsule_passage_clear=%d"),!Blocked);
    const TArray<FVector> Goals={FVector(-6500,5120,0),FVector(6500,5120,0),FVector(-6800,-4620,0),
        FVector(0,-7500,0),FVector(6800,-4620,0),FVector(0,7500,0),FVector(1500,1600,0),
        FVector(3400,3200,0),FVector(2400,3200,0),FVector(1250,3200,0)};
    const bool OK=BuildSaveDesign(W,Path,FVector(0,0,900),FVector(20000,20000,3600),FVector(-1300,6500,0),Goals);
    UE_LOG(LogTemp,Display,TEXT("SIDE_CRASH COMPLETE success=%d breach=(2850,3200) aircraft=(1800,3200) nose=west"),OK);
    if(FParse::Param(FCommandLine::Get(),TEXT("PrototypeMapBatch"))) FPlatformMisc::RequestExit(false);
}
static FAutoConsoleCommand SideCrashCommand(TEXT("PrototypeMap.MoveCrashToSide"),TEXT("Move aircraft and crash breach to the east side of the approved village."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){ MoveCrashToSide(); return false; }),2.f);
    }));
static FAutoConsoleCommand ApprovedCommand(TEXT("PrototypeMap.CreateApproved"),TEXT("Create the approved drawing: rear-wall village with field to the sides and front."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){ CreateLatestVillage(true); return false; }),2.f);
    }));
}
#endif


