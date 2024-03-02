// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MoverBasePawn.h"

#include "MECharacter.generated.h"

struct FInputActionValue;

UCLASS()
class MOVEREXAMPLE_API AMECharacter : public AMoverBasePawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AMECharacter(const FObjectInitializer& ObjectInitializer);

	
	// Whether or not we author our movement inputs relative to whatever base we're standing on, or leave them in world space
	UPROPERTY(BlueprintReadWrite, Category=ME)
	bool bUseBaseRelativeMovement = true;
	
	/**
	 * If true, rotate the Character toward the direction the actor is moving
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ME)
	bool bOrientRotationToMovement = true;
	
	/**
	 * If true, the actor will remain vertical despite any rotation applied to the actor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ME)
	bool bShouldRemainVertical = true;

	/**
	 * If true, the actor will continue orienting towards the last intended orientation (from input) even after movement intent input has ceased.
	 * This makes the character finish orienting after a quick stick flick from the player.  If false, character will not turn without input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ME)
	bool bMaintainLastInputOrientation = false;
};
