#pragma once

// Unreal
#include "GameFramework/PlayerInput.h"
#include "GameFramework/InputSettings.h"

// VR Expansion Plugin
#include "VRBPDatatypes.h"
#include "GripScripts/GS_GunTools.h"

// UHeader Tool
#include "VRGlobalSettings.generated.h"



// Structures

/*namespace ControllerProfileStatics
{
	static const FTransform OculusTouchStaticOffset(FRotator(-70.f, 0.f, 0.f));
}*/


USTRUCT(BlueprintType, Category = "VRGlobalSettings")
struct FAxisMappingDetails
{
	GENERATED_BODY()
public:

	/** List of all axis key mappings that correspond to the axis name in the containing map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FInputAxisKeyMapping> AxisMappings;

};

USTRUCT(BlueprintType, Category = "VRGlobalSettings")
struct FActionMappingDetails
{
	GENERATED_BODY()
public:

	/** List of all axis key mappings that correspond to the axis name in the containing map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
		TArray<FInputActionKeyMapping> ActionMappings;
};

USTRUCT(BlueprintType, Category = "ControllerProfiles")
struct VREXPANSIONPLUGIN_API FBPVRControllerProfile
{
	GENERATED_BODY()
public:

	// Constructors

	FBPVRControllerProfile() :
		ControllerName                  (NAME_None           ),
		SocketOffsetTransform           (FTransform::Identity),
		bUseSeperateHandOffsetTransforms(false               ),
		SocketOffsetTransformRightHand  (FTransform::Identity)
	{}

	FBPVRControllerProfile(FName ControllerName) :
		ControllerName                  (ControllerName      ),
		SocketOffsetTransform           (FTransform::Identity),
		bUseSeperateHandOffsetTransforms(false               ),
		SocketOffsetTransformRightHand  (FTransform::Identity)
	{}

	FBPVRControllerProfile(FName ControllerNameIn, const FTransform & Offset) :
		ControllerName                  (ControllerNameIn    ),
		SocketOffsetTransform           (Offset              ),
		bUseSeperateHandOffsetTransforms(false               ),
		SocketOffsetTransformRightHand  (FTransform::Identity)
	{}

	FBPVRControllerProfile(FName ControllerNameIn, const FTransform & Offset, const FTransform & OffsetRight) :
		ControllerName                  (ControllerNameIn),
		SocketOffsetTransform           (Offset          ),
		bUseSeperateHandOffsetTransforms(true            ),
		SocketOffsetTransformRightHand  (OffsetRight     )
	{}


	// Functions


	// Operator Overloads

	FORCEINLINE bool operator==(const FBPVRControllerProfile &Other) const
	{
		return this->ControllerName == Other.ControllerName;
	}


	// Declares

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControllerProfiles") FName                  ControllerName                  ;   // Name of controller profile
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControllerProfiles") FTransform_NetQuantize SocketOffsetTransform           ;   // Offset to use with this controller.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControllerProfiles") bool                   bUseSeperateHandOffsetTransforms;   // Offset to use with this controller.
	
	UPROPERTY(EditDefaultsOnly, NotReplicated, Category = "ControllerProfiles") TMap<FName, FAxisMappingDetails  > AxisOverrides  ;   // Setting an axis value here with key bindings will override the equivalent bindings on profile load.
	UPROPERTY(EditDefaultsOnly, NotReplicated, Category = "ControllerProfiles") TMap<FName, FActionMappingDetails> ActionOverrides;   // Setting action mappings here will override the equivalent bindings on profile load.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControllerProfiles", meta = (editcondition = "bUseSeperateHandOffsetTransforms")) 
		FTransform_NetQuantize SocketOffsetTransformRightHand;   // Offset to use with this controller.
};



// Classes

UCLASS(config = Engine, defaultconfig)
class VREXPANSIONPLUGIN_API UVRGlobalSettings : public UObject
{
	GENERATED_BODY()

public:

	// Constructors

	UVRGlobalSettings(const FObjectInitializer& ObjectInitializer);


	// Functions

	// UFUNCTION(BlueprintCallable, Category = "GunSettings|VirtualStock")

	// Get the values of the virtual stock settings
	UFUNCTION(BlueprintCallable, Category = "GunSettings|VirtualStock")
	static void GetVirtualStockGlobalSettings(FBPVirtualStockSettings & OutVirtualStockSettings)
	{
		const UVRGlobalSettings& VRSettings = *(GetDefault<UVRGlobalSettings>());

		OutVirtualStockSettings.bUseDistanceBasedStockSnapping = VRSettings.VirtualStockSettings.bUseDistanceBasedStockSnapping;
		OutVirtualStockSettings.StockSnapDistance              = VRSettings.VirtualStockSettings.StockSnapDistance             ;
		OutVirtualStockSettings.StockSnapOffset                = VRSettings.VirtualStockSettings.StockSnapOffset               ;
		OutVirtualStockSettings.bSmoothStockHand               = VRSettings.VirtualStockSettings.bSmoothStockHand              ;
		OutVirtualStockSettings.SmoothingValueForStock         = VRSettings.VirtualStockSettings.SmoothingValueForStock        ;
	}

	// Alter the values of the virtual stock settings and save them out
	UFUNCTION(BlueprintCallable, Category = "GunSettings|VirtualStock")
	static void SaveVirtualStockGlobalSettings(FBPVirtualStockSettings NewVirtualStockSettings)
	{
		UVRGlobalSettings& VRSettings = *(GetMutableDefault<UVRGlobalSettings>());

		VRSettings.VirtualStockSettings = NewVirtualStockSettings;

		VRSettings.SaveConfig();
	}

	// UFUNCTION(BlueprintPure, Category = "VRControllerProfiles")

	/*
	Adjust the transform of a socket for a particular controller model, if a name is not sent in, it will use the currently loaded one.
	If there is no currently loaded one, it will return the input transform as is.
	If bIsRightHand and the target profile uses separate hand transforms it will use the right hand transform.
	*/
	UFUNCTION(BlueprintPure, Category = "VRControllerProfiles")
	static FTransform AdjustTransformByControllerProfile(FName OptionalControllerProfileName, const FTransform & SocketTransform, bool bIsRightHand = false)
	{
		const UVRGlobalSettings& VRSettings = *GetDefault<UVRGlobalSettings>();

		if (OptionalControllerProfileName == NAME_None)
		{
			if (VRSettings.CurrentControllerProfileInUse != NAME_None)
			{
				// Use currently loaded transform
				return SocketTransform * (((bIsRightHand && VRSettings.bUseSeperateHandTransforms) ? VRSettings.CurrentControllerProfileTransformRight : VRSettings.CurrentControllerProfileTransform));
			}

			// No override and no default, return base transform back
			return SocketTransform;
		}

		// Had an override, find it if possible and use its transform
		const FBPVRControllerProfile * FoundProfile = 
			VRSettings.ControllerProfiles.FindByPredicate
			(
				[OptionalControllerProfileName](const FBPVRControllerProfile & ArrayItem)
				{
					return ArrayItem.ControllerName == OptionalControllerProfileName;
				}
			);

		if (FoundProfile)
		{
			return SocketTransform * (((bIsRightHand && VRSettings.bUseSeperateHandTransforms) ? FoundProfile->SocketOffsetTransformRightHand : FoundProfile->SocketOffsetTransform));
		}

		// Couldn't find it, return base transform
		return SocketTransform;
	}

	// Adjust the transform of a socket for a particular controller model
	// If there is no currently loaded one, it will return the input transform as is.
	// If bIsRightHand and the target profile uses separate hand transforms it will use the right hand transform
	UFUNCTION(BlueprintPure, Category = "VRControllerProfiles")
	static FTransform AdjustTransformByGivenControllerProfile(UPARAM(ref) FBPVRControllerProfile & ControllerProfile, const FTransform & SocketTransform, bool bIsRightHand = false)
	{
		// Use currently loaded transform
		return SocketTransform * (((bIsRightHand && ControllerProfile.bUseSeperateHandOffsetTransforms) ? ControllerProfile.SocketOffsetTransformRightHand : ControllerProfile.SocketOffsetTransform));

		// Couldn't find it, return base transform
		return SocketTransform;
	}

	// UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles")

	// Get array of controller profiles
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles")
	static TArray<FBPVRControllerProfile> GetControllerProfiles()
	{
		const UVRGlobalSettings& VRSettings = *GetDefault<UVRGlobalSettings>();

		return VRSettings.ControllerProfiles;
	}


	// Get name of currently loaded profile (if one is loaded)
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles")
	static FName GetCurrentProfileName(bool & bHadLoadedProfile)
	{
		const UVRGlobalSettings& VRSettings = *GetDefault<UVRGlobalSettings>();

		bHadLoadedProfile = VRSettings.CurrentControllerProfileInUse != NAME_None;

		return VRSettings.CurrentControllerProfileInUse;
	}

	// Get name of currently loaded profile (if one is loaded)
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles")
	static FBPVRControllerProfile GetCurrentProfile(bool & bHadLoadedProfile)
	{
		const UVRGlobalSettings& VRSettings            = *GetDefault<UVRGlobalSettings>        ();
		      FName              ControllerProfileName = VRSettings.CurrentControllerProfileInUse;

		const FBPVRControllerProfile* FoundProfile = 
			VRSettings.ControllerProfiles.FindByPredicate
			(
				[ControllerProfileName](const FBPVRControllerProfile & ArrayItem)
				{
					return ArrayItem.ControllerName == ControllerProfileName;
				}
			);

		bHadLoadedProfile = FoundProfile != nullptr;

		if (bHadLoadedProfile)
		{
			return *FoundProfile;
		}
		else
		{
			return FBPVRControllerProfile();
		}
	}

	// Get a controller profile by name
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles")
	static bool GetControllerProfile(FName ControllerProfileName, FBPVRControllerProfile & OutProfile)
	{
		const UVRGlobalSettings& VRSettings = *GetDefault<UVRGlobalSettings>();

		const FBPVRControllerProfile * FoundProfile =
			VRSettings.ControllerProfiles.FindByPredicate
			(
				[ControllerProfileName](const FBPVRControllerProfile & ArrayItem)
				{
					return ArrayItem.ControllerName == ControllerProfileName;
				}
			);

		if (FoundProfile)
		{
			OutProfile = *FoundProfile;

			return true;
		}

		return false;
	}

	/*
	Load a controller profile by name:

	Action / Axis mappings overwrite ones with the same action/axis name in the input settings.
	If you have an action/axis override but don't assign buttons to it then it will just delete it.
	This can be useful for removing actions and using multiple actions (IE: Grip Touch, Grip Vive actions).
	For when just changing the buttons isn't good enough.
	If bSetDefaults is true, will set this as the currently loaded profile.
	Otherwise will just load it (useful for Left/Right handed profile additions and the like to have it false).
	*/
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles")
	static bool LoadControllerProfileByName(FName ControllerProfileName, bool bSetAsCurrentProfile = true)
	{
		const UVRGlobalSettings& VRSettings = *GetDefault<UVRGlobalSettings>();

		const FBPVRControllerProfile * FoundProfile = 
			VRSettings.ControllerProfiles.FindByPredicate
			(
				[ControllerProfileName](const FBPVRControllerProfile & ArrayItem)
				{
					return ArrayItem.ControllerName == ControllerProfileName;
				}
			);

		if (FoundProfile)
		{
			return LoadControllerProfile(*FoundProfile, bSetAsCurrentProfile);
		}

		return false;
	}

	/*
	Load a controller profile:

	Action / Axis mappings overwrite ones with the same action/axis name in the input settings.
	If you have an action/axis override but don't assign buttons to it then it will just delete it.
	This can be useful for removing actions and using multiple actions. (IE: Grip Touch, Grip Vive actions)
	For when just changing the buttons isn't good enough.
	If bSetDefaults is true, will set this as the currently loaded profile.
	Otherwise will just load it (useful for Left/Right handed profile additions and the like to have it false).
	*/
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles")
	static bool LoadControllerProfile(const FBPVRControllerProfile & ControllerProfile, bool bSetAsCurrentProfile = true)
	{
		UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();

		if (InputSettings != nullptr)
		{
			if (ControllerProfile.ActionOverrides.Num() > 0)
			{
				// Load button mappings.
				for (auto& Elem : ControllerProfile.ActionOverrides)
				{
					FName                 ActionName = Elem.Key  ;
					FActionMappingDetails Mapping    = Elem.Value;

					// We allow for 0 mapped actions here in case you want to delete one.
					if (ActionName == NAME_None /*|| Mapping.ActionMappings.Num() < 1*/)
					{
						continue;
					}

					// Clear all actions that use our action name first.
					for (int32 ActionIndex = InputSettings->ActionMappings.Num() - 1; ActionIndex >= 0; --ActionIndex)
					{
						if (InputSettings->ActionMappings[ActionIndex].ActionName == ActionName)
						{
							InputSettings->ActionMappings.RemoveAt(ActionIndex);   // We don't break because the mapping may have been in the array twice.
						}
					}

					// Then add the new bindings
					for (FInputActionKeyMapping &KeyMapping : Mapping.ActionMappings)
					{
						// By default the key mappings don't have an action name, add them here.

						KeyMapping.ActionName = ActionName;

						InputSettings->ActionMappings.Add(KeyMapping);
					}
				}
			}

			if (ControllerProfile.AxisOverrides.Num() > 0)
			{
				// Load axis mappings.
				for (auto& Elem : ControllerProfile.AxisOverrides)
				{
					FName               AxisName = Elem.Key  ;
					FAxisMappingDetails Mapping  = Elem.Value;

					// We allow for 0 mapped Axis's here in case you want to delete one.
					if (AxisName == NAME_None /*|| Mapping.AxisMappings.Num() < 1*/)
					{
						continue;
					}

					// Clear all Axis's that use our Axis name first.
					for (int32 AxisIndex = InputSettings->AxisMappings.Num() - 1; AxisIndex >= 0; --AxisIndex)
					{
						if (InputSettings->AxisMappings[AxisIndex].AxisName == AxisName)
						{
							InputSettings->AxisMappings.RemoveAt(AxisIndex);   // We don't break because the mapping may have been in the array twice.
							
						}
					}

					// Then add the new bindings.
					for (FInputAxisKeyMapping &KeyMapping : Mapping.AxisMappings)
					{
						// By default the key mappings don't have an Axis name, add them here.

						KeyMapping.AxisName = AxisName;

						InputSettings->AxisMappings.Add(KeyMapping);
					}
				}
			}

			if (ControllerProfile.ActionOverrides.Num() > 0 || ControllerProfile.AxisOverrides.Num() > 0)
			{
				InputSettings->ForceRebuildKeymaps();   // Tell all players to use the new key mappings.
			}
		}

		if (bSetAsCurrentProfile)
		{
			UVRGlobalSettings* VRSettings = GetMutableDefault<UVRGlobalSettings>();
			if (VRSettings)
			{
				VRSettings->CurrentControllerProfileInUse     = ControllerProfile.ControllerName       ;
				VRSettings->CurrentControllerProfileTransform = ControllerProfile.SocketOffsetTransform;

				ensure(!VRSettings->CurrentControllerProfileTransform.ContainsNaN());
				
				VRSettings->bUseSeperateHandTransforms             = ControllerProfile.bUseSeperateHandOffsetTransforms;
				VRSettings->CurrentControllerProfileTransformRight = ControllerProfile.SocketOffsetTransformRightHand  ;
				
				ensure(!VRSettings->CurrentControllerProfileTransformRight.ContainsNaN());
				
				VRSettings->OnControllerProfileChangedEvent.Broadcast();
			}
			else
			{
				return false;
			}
		}
		
		return true;   // Not saving key mapping in purpose, app will revert to default on next load and profiles will load custom changes.
	}

	// UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles|Operations")

	// Overwrite a controller profile
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles|Operations")
	static void OverwriteControllerProfile(UPARAM(ref)FBPVRControllerProfile &OverwritingProfile, bool bSaveOutToConfig = true)
	{
		UVRGlobalSettings& VRSettings = *GetMutableDefault<UVRGlobalSettings>();

		for (int profileIndex = 0; profileIndex < VRSettings.ControllerProfiles.Num(); ++profileIndex)
		{
			if (VRSettings.ControllerProfiles[profileIndex].ControllerName == OverwritingProfile.ControllerName)
			{
				VRSettings.ControllerProfiles[profileIndex] = OverwritingProfile;
			}
		}

		if (bSaveOutToConfig)
		{
			SaveControllerProfiles();
		}
	}

	// Add a controller profile
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles|Operations")
	static void AddControllerProfile(UPARAM(ref)FBPVRControllerProfile &NewProfile, bool bSaveOutToConfig = true)
	{
		UVRGlobalSettings& VRSettings = *GetMutableDefault<UVRGlobalSettings>();

		VRSettings.ControllerProfiles.Add(NewProfile);

		if (bSaveOutToConfig)
		{
			SaveControllerProfiles();
		}
	}

	// Overwrite a controller profile
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles|Operations")
	static void DeleteControllerProfile(FName ControllerProfileName, bool bSaveOutToConfig = true)
	{
		UVRGlobalSettings& VRSettings = *GetMutableDefault<UVRGlobalSettings>();

		for (int profileIndex = VRSettings.ControllerProfiles.Num() - 1; profileIndex >= 0; --profileIndex)
		{
			if (VRSettings.ControllerProfiles[profileIndex].ControllerName == ControllerProfileName)
			{
				VRSettings.ControllerProfiles.RemoveAt(profileIndex);
			}
		}

		if (bSaveOutToConfig)
		{
			SaveControllerProfiles();
		}
	}

	// Saved the VRGlobalSettings out to its config file, will include any alterations that you made to the profile
	UFUNCTION(BlueprintCallable, Category = "VRControllerProfiles|Operations")
	static void SaveControllerProfiles()
	{
		UVRGlobalSettings& VRSettings = *GetMutableDefault<UVRGlobalSettings>();

		VRSettings.SaveConfig();

		//VRSettings.SaveConfig(CPF_Config, *VRSettings.GetGlobalUserConfigFilename());//VRSettings.GetDefaultConfigFilename());
	}

	// UObject Overloads

	virtual void PostInitProperties() override
	{
		#if WITH_EDITOR
				// Not doing this currently, loading defaults is cool, and I may go back to it later when i get
				// controller offsets for vive/touch/MR vs each other.
				/*if (ControllerProfiles.Num() == 0)
				{
					ControllerProfiles.Add(FBPVRControllerProfile(TEXT("Vive_Wands")));
					ControllerProfiles.Add(FBPVRControllerProfile(TEXT("Oculus_Touch"), ControllerProfileStatics::OculusTouchStaticOffset));
					this->SaveConfig(CPF_Config, *this->GetDefaultConfigFilename());
				}*/
		#endif


		Super::PostInitProperties();
	}


	// Declares

	UPROPERTY(config, EditAnywhere, Category = "GunSettings") FBPVirtualStockSettings VirtualStockSettings;
	
	UPROPERTY(config, EditAnywhere, Category = "GunSettings|Secondary Grip 1Euro Settings") float OneEuroMinCutoff  ;   // Setting to use for the OneEuro smoothing low pass filter when double gripping something held with a hand.
	UPROPERTY(config, EditAnywhere, Category = "GunSettings|Secondary Grip 1Euro Settings") float OneEuroCutoffSlope;   // Setting to use for the OneEuro smoothing low pass filter when double gripping something held with a hand.
	UPROPERTY(config, EditAnywhere, Category = "GunSettings|Secondary Grip 1Euro Settings") float OneEuroDeltaCutoff;   // Setting to use for the OneEuro smoothing low pass filter when double gripping something held with a hand.

	/** Delegate for notification when the controller profile changes. */
	DECLARE_MULTICAST_DELEGATE(FVRControllerProfileChangedEvent); 
	FVRControllerProfileChangedEvent OnControllerProfileChangedEvent;

	// Controller profiles to store related information on a per profile basis
	UPROPERTY(config, EditAnywhere, Category = "ControllerProfiles") TArray<FBPVRControllerProfile> ControllerProfiles;

	// Store these to save some processing when getting the transform after a profile is loaded
	FName      CurrentControllerProfileInUse         ;
	FTransform CurrentControllerProfileTransform     ;
	bool       bUseSeperateHandTransforms            ;
	FTransform CurrentControllerProfileTransformRight;
};
