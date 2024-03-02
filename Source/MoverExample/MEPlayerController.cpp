// Fill out your copyright notice in the Description page of Project Settings.


#include "MEPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputTriggers.h"
#include "MECharacter.h"
#include "MoverBasePawn.h"
#include "MoverComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"


AMEPlayerController::AMEPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMEPlayerController::BeginPlay()
{
	Super::BeginPlay();

	PlayerCameraManager->ViewPitchMax = 89.0f;
	PlayerCameraManager->ViewPitchMin = -89.0f;
}


void AMEPlayerController::PlayerTick(float DeltaTime)
{
	// Spin camera based on input
	// Simple input scaling. A real game will probably map this to an acceleration curve
	static float LookRateYaw = 100.f;	// degs/sec
	static float LookRatePitch = 100.f;	// degs/sec

	AddYawInput(CachedLookInput.Yaw * LookRateYaw * DeltaTime);
	AddPitchInput(-CachedLookInput.Pitch * LookRatePitch * DeltaTime);

	// Clear all camera-related cached input
	CachedLookInput = FRotator::ZeroRotator;
	
	Super::PlayerTick(DeltaTime);
}

void AMEPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(InputMapping, 0);
	}
	
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AMEPlayerController::OnMoveTriggered);
		EnhancedInputComponent->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &AMEPlayerController::OnMoveCompleted);
		EnhancedInputComponent->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AMEPlayerController::OnLookTriggered);
		EnhancedInputComponent->BindAction(LookInputAction, ETriggerEvent::Completed, this, &AMEPlayerController::OnLookCompleted);
		EnhancedInputComponent->BindAction(JumpInputAction, ETriggerEvent::Started, this, &AMEPlayerController::OnJumpStarted);
		EnhancedInputComponent->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &AMEPlayerController::OnJumpReleased);
		EnhancedInputComponent->BindAction(FlyInputAction, ETriggerEvent::Triggered, this, &AMEPlayerController::OnFlyTriggered);
	}
}


void AMEPlayerController::OnPossess(APawn *InPawn)
{
	Super::OnPossess(InPawn);

	if (const AMECharacter* const MECharacter = GetMECharacter())
	{
		if (UMoverComponent* const MoverComponent = MECharacter->GetMoverComponent())
		{
			MoverComponent->InputProducer = this;
		}
	}
}

void AMEPlayerController::OnUnPossess()
{
	if (const AMECharacter* const MECharacter = GetMECharacter())
	{
		if (UMoverComponent* const MoverComponent = MECharacter->GetMoverComponent())
		{
			MoverComponent->InputProducer = nullptr;
		}
	}

	Super::OnUnPossess();
}

void AMEPlayerController::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext &InputCmdResult)
{
	float DeltaMS = static_cast<float>(SimTimeMs);
	
	// Generate user commands. Called right before the Character movement simulation will tick (for a locally controlled pawn)
	// This isn't meant to be the best way of doing a camera system. It is just meant to show a couple of ways it may be done
	// and to make sure we can keep distinct the movement, rotation, and view angles.
	// Styles 1-3 are really meant to be used with a gamepad.
	//
	// Its worth calling out: the code that happens here is happening *outside* of the Character movement simulation. All we are doing
	// is generating the input being fed into that simulation. That said, this means that A) the code below does not run on the server
	// (and non controlling clients) and B) the code is not rerun during reconcile/resimulates. Use this information guide any
	// decisions about where something should go (such as aim assist, lock on targeting systems, etc): it is hard to give absolute
	// answers and will depend on the game and its specific needs. In general, at this time, I'd recommend aim assist and lock on 
	// targeting systems to happen /outside/ of the system, i.e, here. But I can think of scenarios where that may not be ideal too.

	const AMECharacter* MECharacter = GetMECharacter();

	FCharacterDefaultInputs& CharacterInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	CharacterInputs.ControlRotation = GetControlRotation();

	// Favor velocity input 
	bool bUsingInputIntentForMove = CachedMoveInputVelocity.IsZero();
	if (bUsingInputIntentForMove)
	{
		FRotator Rotator = CharacterInputs.ControlRotation;
		FVector FinalDirectionalIntent;
		if (const UMoverComponent* MoverComponent = MECharacter->GetMoverComponent())
		{
			if (MoverComponent->IsOnGround() || MoverComponent->IsFalling())
			{
				const FVector RotationProjectedOntoUpDirection = FVector::VectorPlaneProject(Rotator.Vector(), MoverComponent->GetUpDirection()).GetSafeNormal();
				Rotator = RotationProjectedOntoUpDirection.Rotation();
			}

			FinalDirectionalIntent = Rotator.RotateVector(CachedMoveInputIntent);
		}
		
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);
	}
	else
	{
		CharacterInputs.SetMoveInput(EMoveInputType::Velocity, CachedMoveInputVelocity);
	}

	static float RotationMagMin(1e-3);

	const bool bHasAffirmativeMoveInput = (CharacterInputs.GetMoveInput().Size() >= RotationMagMin);
	
	// Figure out intended orientation
	CharacterInputs.OrientationIntent = FVector::ZeroVector;


	if (bUsingInputIntentForMove && bHasAffirmativeMoveInput)
	{
		if (MECharacter->bOrientRotationToMovement)
		{
			// set the intent to the actors movement direction
			CharacterInputs.OrientationIntent = CharacterInputs.GetMoveInput();
		}
		else
		{
			// set intent to the the control rotation - often a player's camera rotation
			CharacterInputs.OrientationIntent = CharacterInputs.ControlRotation.Vector();
		}

		LastAffirmativeMoveInput = CharacterInputs.GetMoveInput();

	}
	else if (MECharacter->bMaintainLastInputOrientation)
	{
		// There is no movement intent, so use the last-known affirmative move input
		CharacterInputs.OrientationIntent = LastAffirmativeMoveInput;
	}
	
	if (MECharacter->bShouldRemainVertical)
	{
		// canceling out any z intent if the actor is supposed to remain vertical
		CharacterInputs.OrientationIntent = CharacterInputs.OrientationIntent.GetSafeNormal2D();
	}

	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;

	if (bShouldToggleFlying)
	{
		if (!bIsFlyingActive)
		{
			CharacterInputs.SuggestedMovementMode = DefaultModeNames::Flying;
		}
		else
		{
			CharacterInputs.SuggestedMovementMode = DefaultModeNames::Falling;
		}

		bIsFlyingActive = !bIsFlyingActive;
	}
	else
	{
		CharacterInputs.SuggestedMovementMode = NAME_None;
	}

	// Convert inputs to be relative to the current movement base (depending on options and state)
	CharacterInputs.bUsingMovementBase = false;

	if (MECharacter->bUseBaseRelativeMovement)
	{
		if (const UMoverComponent* MoverComp = GetComponentByClass<UMoverComponent>())
		{
			if (UPrimitiveComponent* MovementBase = MoverComp->GetMovementBase())
			{
				FName MovementBaseBoneName = MoverComp->GetMovementBaseBoneName();

				FVector RelativeMoveInput, RelativeOrientDir;

				UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.GetMoveInput(), RelativeMoveInput);
				UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.OrientationIntent, RelativeOrientDir);

				CharacterInputs.SetMoveInput(CharacterInputs.GetMoveInputType(), RelativeMoveInput);
				CharacterInputs.OrientationIntent = RelativeOrientDir;

				CharacterInputs.bUsingMovementBase = true;
				CharacterInputs.MovementBase = MovementBase;
				CharacterInputs.MovementBaseBoneName = MovementBaseBoneName;
			}
		}
	}

	// Clear/consume temporal movement inputs. We are not consuming others in the event that the game world is ticking at a lower rate than the Mover simulation. 
	// In that case, we want most input to carry over between simulation frames.
	{

		bIsJumpJustPressed = false;
		bShouldToggleFlying = false;
	}
}

AMECharacter* AMEPlayerController::GetMECharacter() const
{
	return Cast<AMECharacter>(GetPawn());
}

void AMEPlayerController::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector MovementVector = -Value.Get<FVector>();
	CachedMoveInputIntent.X = FMath::Clamp(MovementVector.X, -1.0f, 1.0f);
	CachedMoveInputIntent.Y = FMath::Clamp(MovementVector.Y, -1.0f, 1.0f);
	CachedMoveInputIntent.Z = FMath::Clamp(MovementVector.Z, -1.0f, 1.0f);
}

void AMEPlayerController::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void AMEPlayerController::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	CachedLookInput.Yaw = CachedTurnInput.Yaw = FMath::Clamp(LookVector.X, -1.0f, 1.0f);
	CachedLookInput.Pitch = CachedTurnInput.Pitch = FMath::Clamp(LookVector.Y, -1.0f, 1.0f);
}

void AMEPlayerController::OnLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput = FRotator::ZeroRotator;
	CachedTurnInput = FRotator::ZeroRotator;
}

void AMEPlayerController::OnJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void AMEPlayerController::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

void AMEPlayerController::OnFlyTriggered(const FInputActionValue& Value)
{
	bShouldToggleFlying = true;
}
