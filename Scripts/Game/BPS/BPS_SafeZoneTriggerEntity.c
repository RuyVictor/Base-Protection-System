//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
//
// CHARACTER CORE
//
// - Native spherical trigger
// - Direct character detection
// - Vehicle occupant detection
// - Friendly enter/exit messages
// - Enemy countdown + execution
// - Character Safe Zone protection
// - Friendly fire blocking only when BOTH players are inside
// - Combat Lock after firing
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
// Runtime state of one character.
//------------------------------------------------------------------------------------------------
class BPS_CharacterState
{
	IEntity m_Character;

	// Character itself is detected by the trigger.
	bool m_bDirectInside;

	// Character is occupying a vehicle detected by the trigger.
	bool m_bVehicleInside;

	// Consolidated state from previous tick.
	bool m_bWasInside;

	// Enemy intrusion
	int m_iIntruderStartTime;
	int m_iLastIntruderSecond;

	// Combat lock
	int m_iCombatLockStartTime;

	// Friendly fire message cooldown
	int m_iFriendlyFireMessageTime;

	// Weapon tracking
	BaseWeaponComponent m_Weapon;
	BaseMuzzleComponent m_Muzzle;

	int m_iLastAmmo;
	bool m_bWeaponInitialized;


	//------------------------------------------------------------------------------------------------
	void BPS_CharacterState(IEntity character)
	{
		m_Character = character;

		m_bDirectInside = false;
		m_bVehicleInside = false;
		m_bWasInside = false;

		m_iIntruderStartTime = -1;
		m_iLastIntruderSecond = -1;

		m_iCombatLockStartTime = -1;

		m_iFriendlyFireMessageTime = -1;

		m_Weapon = null;
		m_Muzzle = null;

		m_iLastAmmo = -1;
		m_bWeaponInitialized = false;
	}


	//------------------------------------------------------------------------------------------------
	bool IsInside()
	{
		return (
			m_bDirectInside ||
			m_bVehicleInside
		);
	}
}


//------------------------------------------------------------------------------------------------
[EntityEditorProps(
	category: "BPS/Base Protection System",
	description: "Spherical Base Protection System Safe Zone."
)]
class BPS_SafeZoneTriggerEntityClass : SCR_BaseTriggerEntityClass
{
}


//------------------------------------------------------------------------------------------------
class BPS_SafeZoneTriggerEntity : SCR_BaseTriggerEntity
{
	// =============================================================================================
	// INTERNAL
	// =============================================================================================

	protected static const float BPS_TRIGGER_UPDATE_RATE = 0.25;

	protected static const int BPS_LOGIC_INTERVAL_MS = 100;

	protected static const float BPS_COUNTDOWN_HINT_DURATION = 1.1;

	protected static const int BPS_FRIENDLY_FIRE_MESSAGE_COOLDOWN_MS = 2000;


	// =============================================================================================
	// GLOBAL ZONE REGISTRY
	// =============================================================================================

	protected static ref array<BPS_SafeZoneTriggerEntity> s_aZones =
		new array<BPS_SafeZoneTriggerEntity>();


	// =============================================================================================
	// CONFIGURATION
	// =============================================================================================

	[Attribute(
		"US",
		UIWidgets.EditBox,
		"Protected faction key. Vanilla: US or USSR."
	)]
	protected FactionKey m_sProtectedFactionKey;


	// =============================================================================================
	// FRIENDLY UI
	// =============================================================================================

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show enter and exit messages to friendly players."
	)]
	protected bool m_bShowFriendlyEnterExitMessages;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sFriendlyEnterTitle;


	[Attribute(
		"You entered a safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sFriendlyEnterMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sFriendlyExitTitle;


	[Attribute(
		"You left the safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sFriendlyExitMessage;


	// =============================================================================================
	// ENEMY
	// =============================================================================================

	[Attribute(
		"10",
		UIWidgets.EditBox,
		"Seconds an enemy can remain inside before being killed."
	)]
	protected int m_iEnemyKillDelaySeconds;


	[Attribute(
		"WARNING - ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sEnemyWarningTitle;


	[Attribute(
		"You are inside an enemy Safe Zone. Leave immediately. You will be killed in %1 seconds.",
		UIWidgets.EditBox,
		"Supports localization keys. %1 = remaining seconds."
	)]
	protected string m_sEnemyWarningMessage;


	[Attribute(
		"ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sEnemyExitTitle;


	[Attribute(
		"You left the enemy Safe Zone. Elimination cancelled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sEnemyExitMessage;


	// =============================================================================================
	// COMBAT LOCK
	// =============================================================================================

	[Attribute(
		"15",
		UIWidgets.EditBox,
		"Seconds without Safe Zone protection after firing. 0 disables Combat Lock."
	)]
	protected float m_fCombatLockSeconds;


	[Attribute(
		"SAFE ZONE - COMBAT",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sCombatLockTitle;


	[Attribute(
		"You fired inside the Safe Zone. Your protection has been temporarily disabled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sCombatLockMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sProtectionRestoredTitle;


	[Attribute(
		"Your Safe Zone protection has been restored.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sProtectionRestoredMessage;


	// =============================================================================================
	// FRIENDLY FIRE
	// =============================================================================================

	[Attribute(
		"FRIENDLY FIRE BLOCKED",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sFriendlyFireTitle;


	[Attribute(
		"You cannot damage allies while both players are inside the Safe Zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sFriendlyFireMessage;


	// =============================================================================================
	// UI
	// =============================================================================================

	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Normal message duration."
	)]
	protected float m_fMessageDuration;


	// =============================================================================================
	// DEBUG
	// =============================================================================================

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Enable BPS debug logs."
	)]
	protected bool m_bDebug;


	// =============================================================================================
	// RUNTIME
	// =============================================================================================

	protected ref array<ref BPS_CharacterState> m_aStates =
		new array<ref BPS_CharacterState>();


	// Vehicles currently detected by the native sphere trigger.
	protected ref array<IEntity> m_aVehiclesInside =
		new array<IEntity>();


	// Reused occupant buffer.
	protected ref array<IEntity> m_aOccupants =
		new array<IEntity>();


	// =============================================================================================
	// INIT
	// =============================================================================================

	override void OnInit(IEntity owner)
	{
		super.OnInit(owner);


		if (m_sProtectedFactionKey == "URSS")
		{
			m_sProtectedFactionKey = "USSR";

			Print(
				"[BPS] URSS converted to USSR.",
				LogLevel.WARNING
			);
		}


		if (m_iEnemyKillDelaySeconds < 1)
			m_iEnemyKillDelaySeconds = 1;


		if (m_fCombatLockSeconds < 0)
			m_fCombatLockSeconds = 0;


		SetUpdateRate(
			BPS_TRIGGER_UPDATE_RATE
		);


		// Server-side detection only.
		if (
			Replication.IsRunning() &&
			!Replication.IsServer()
		)
		{
			EnablePeriodicQueries(false);

			return;
		}


		EnablePeriodicQueries(true);


		if (s_aZones.Find(this) < 0)
			s_aZones.Insert(this);


		GetGame().GetCallqueue().CallLater(
			BPS_LogicTick,
			BPS_LOGIC_INTERVAL_MS,
			true
		);


		DebugLog(
			string.Format(
				"INIT | Faction=%1 | Radius=%2",
				m_sProtectedFactionKey,
				GetSphereRadius()
			)
		);
	}


	// =============================================================================================
	// TRIGGER FILTER
	//
	// We now accept:
	//
	// 1. Characters
	// 2. Vehicles which contain a SCR_BaseCompartmentManagerComponent
	// =============================================================================================

	override bool ScriptedEntityFilterForQuery(IEntity ent)
	{
		if (!ent)
			return false;


		if (ChimeraCharacter.Cast(ent))
			return true;


		return GetVehicleCompartmentManager(ent) != null;
	}


	// =============================================================================================
	// TRIGGER ENTER
	// =============================================================================================

	override void OnActivate(IEntity ent)
	{
		super.OnActivate(ent);


		if (!ent)
			return;


		// =========================================================================================
		// CHARACTER DIRECTLY INSIDE
		// =========================================================================================

		ChimeraCharacter character =
			ChimeraCharacter.Cast(ent);


		if (character)
		{
			BPS_CharacterState state =
				GetOrCreateState(character);


			state.m_bDirectInside = true;


			DebugLog(
				string.Format(
					"DIRECT ACTIVATE | Character=%1",
					character
				)
			);


			return;
		}


		// =========================================================================================
		// VEHICLE INSIDE
		// =========================================================================================

		SCR_BaseCompartmentManagerComponent manager =
			GetVehicleCompartmentManager(ent);


		if (!manager)
			return;


		if (
			m_aVehiclesInside.Find(ent) < 0
		)
		{
			m_aVehiclesInside.Insert(ent);
		}


		DebugLog(
			string.Format(
				"VEHICLE ACTIVATE | Vehicle=%1 | Occupants=%2",
				ent,
				manager.GetOccupantCount()
			)
		);
	}


	// =============================================================================================
	// TRIGGER EXIT
	// =============================================================================================

	override void OnDeactivate(IEntity ent)
	{
		super.OnDeactivate(ent);


		if (!ent)
			return;


		// =========================================================================================
		// CHARACTER DIRECTLY LEFT
		// =========================================================================================

		ChimeraCharacter character =
			ChimeraCharacter.Cast(ent);


		if (character)
		{
			BPS_CharacterState state =
				FindState(character);


			if (state)
			{
				state.m_bDirectInside = false;
			}


			DebugLog(
				string.Format(
					"DIRECT DEACTIVATE | Character=%1",
					character
				)
			);


			return;
		}


		// =========================================================================================
		// VEHICLE LEFT
		// =========================================================================================

		int vehicleIndex =
			m_aVehiclesInside.Find(ent);


		if (vehicleIndex >= 0)
		{
			m_aVehiclesInside.Remove(
				vehicleIndex
			);
		}


		DebugLog(
			string.Format(
				"VEHICLE DEACTIVATE | Vehicle=%1",
				ent
			)
		);
	}


	// =============================================================================================
	// MAIN LOOP
	// =============================================================================================

	protected void BPS_LogicTick()
	{
		// =========================================================================================
		// STEP 1
		//
		// Reset vehicle-presence flag.
		//
		// It will be rebuilt from vehicles currently inside the trigger.
		// =========================================================================================

		foreach (
			BPS_CharacterState state :
			m_aStates
		)
		{
			if (state)
				state.m_bVehicleInside = false;
		}


		// =========================================================================================
		// STEP 2
		//
		// Get every occupant of every vehicle currently inside.
		// =========================================================================================

		for (int v = m_aVehiclesInside.Count() - 1; v >= 0; v--)
		{
			IEntity vehicle =
				m_aVehiclesInside[v];


			if (!vehicle)
			{
				m_aVehiclesInside.Remove(v);

				continue;
			}


			SCR_BaseCompartmentManagerComponent manager =
				GetVehicleCompartmentManager(
					vehicle
				);


			if (!manager)
			{
				m_aVehiclesInside.Remove(v);

				continue;
			}


			m_aOccupants.Clear();


			manager.GetOccupants(
				m_aOccupants
			);


			foreach (
				IEntity occupant :
				m_aOccupants
			)
			{
				ChimeraCharacter occupantCharacter =
					ChimeraCharacter.Cast(
						occupant
					);


				if (!occupantCharacter)
					continue;


				BPS_CharacterState occupantState =
					GetOrCreateState(
						occupantCharacter
					);


				occupantState.m_bVehicleInside =
					true;
			}
		}


		// =========================================================================================
		// STEP 3
		//
		// Consolidate:
		//
		// directInside || vehicleInside
		// =========================================================================================

		for (int i = m_aStates.Count() - 1; i >= 0; i--)
		{
			BPS_CharacterState state =
				m_aStates[i];


			if (!state)
			{
				m_aStates.Remove(i);

				continue;
			}


			IEntity character =
				state.m_Character;


			if (!character)
			{
				m_aStates.Remove(i);

				continue;
			}


			bool isInside =
				state.IsInside();


			// =====================================================================================
			// ENTER TRANSITION
			// =====================================================================================

			if (
				isInside &&
				!state.m_bWasInside
			)
			{
				state.m_bWasInside = true;


				HandleCharacterEntered(
					state
				);
			}


			// =====================================================================================
			// EXIT TRANSITION
			// =====================================================================================

			else if (
				!isInside &&
				state.m_bWasInside
			)
			{
				state.m_bWasInside = false;


				HandleCharacterExited(
					state
				);
			}


			// =====================================================================================
			// ENEMY
			// =====================================================================================

			if (
				isInside &&
				IsIntruderActive(state)
			)
			{
				ProcessIntruder(
					state
				);
			}


			// =====================================================================================
			// FRIENDLY WEAPON / COMBAT LOCK
			// =====================================================================================

			else if (
				isInside &&
				IsProtectedFaction(character)
			)
			{
				if (
					m_fCombatLockSeconds > 0 &&
					HasFired(state)
				)
				{
					ApplyCombatLock(
						state
					);
				}


				if (
					state.m_iCombatLockStartTime >= 0 &&
					!IsCombatLocked(state)
				)
				{
					state.m_iCombatLockStartTime =
						-1;


					ShowMessage(
						character,
						m_sProtectionRestoredTitle,
						m_sProtectionRestoredMessage
					);
				}
			}


			// =====================================================================================
			// CLEANUP
			// =====================================================================================

			if (
				!isInside &&
				!IsIntruderActive(state) &&
				!IsCombatLocked(state)
			)
			{
				m_aStates.Remove(i);
			}
		}
	}


	// =============================================================================================
	// CHARACTER TRANSITIONS
	// =============================================================================================

	protected void HandleCharacterEntered(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		IEntity character =
			state.m_Character;


		DebugLog(
			string.Format(
				"ENTER | Character=%1 | Faction=%2 | Direct=%3 | Vehicle=%4",
				character,
				GetFactionKey(character),
				state.m_bDirectInside,
				state.m_bVehicleInside
			)
		);


		// =========================================================================================
		// FRIENDLY
		// =========================================================================================

		if (IsProtectedFaction(character))
		{
			ResetIntruder(state);

			InitializeWeaponTracking(state);


			if (
				m_bShowFriendlyEnterExitMessages
			)
			{
				ShowMessage(
					character,
					m_sFriendlyEnterTitle,
					m_sFriendlyEnterMessage
				);
			}


			return;
		}


		// =========================================================================================
		// ENEMY
		// =========================================================================================

		StartIntruderCountdown(
			state
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void HandleCharacterExited(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		IEntity character =
			state.m_Character;


		DebugLog(
			string.Format(
				"EXIT | Character=%1 | Faction=%2",
				character,
				GetFactionKey(character)
			)
		);


		ResetWeaponTracking(state);


		// =========================================================================================
		// ENEMY EXIT
		// =========================================================================================

		if (IsIntruderActive(state))
		{
			ResetIntruder(state);


			ShowMessage(
				character,
				m_sEnemyExitTitle,
				m_sEnemyExitMessage
			);


			return;
		}


		// =========================================================================================
		// FRIENDLY EXIT
		// =========================================================================================

		if (
			IsProtectedFaction(character) &&
			m_bShowFriendlyEnterExitMessages
		)
		{
			ShowMessage(
				character,
				m_sFriendlyExitTitle,
				m_sFriendlyExitMessage
			);
		}
	}


	// =============================================================================================
	// VEHICLE
	// =============================================================================================

	protected SCR_BaseCompartmentManagerComponent GetVehicleCompartmentManager(
		IEntity ent
	)
	{
		if (!ent)
			return null;


		return SCR_BaseCompartmentManagerComponent.Cast(
			ent.FindComponent(
				SCR_BaseCompartmentManagerComponent
			)
		);
	}


	// =============================================================================================
	// ENEMY COUNTDOWN
	// =============================================================================================

	protected void StartIntruderCountdown(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		state.m_iIntruderStartTime =
			System.GetTickCount();


		state.m_iLastIntruderSecond =
			m_iEnemyKillDelaySeconds;


		ShowMessageParam1(
			state.m_Character,
			m_sEnemyWarningTitle,
			m_sEnemyWarningMessage,
			m_iEnemyKillDelaySeconds,
			BPS_COUNTDOWN_HINT_DURATION
		);


		DebugLog(
			string.Format(
				"INTRUDER START | Entity=%1 | Seconds=%2",
				state.m_Character,
				m_iEnemyKillDelaySeconds
			)
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ProcessIntruder(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		IEntity character =
			state.m_Character;


		if (!character)
			return;


		// Faction changed while inside.
		if (IsProtectedFaction(character))
		{
			ResetIntruder(state);

			InitializeWeaponTracking(state);

			return;
		}


		int elapsedMs =
			System.GetTickCount(
				state.m_iIntruderStartTime
			);


		int totalMs =
			m_iEnemyKillDelaySeconds *
			1000;


		int remainingMs =
			totalMs -
			elapsedMs;


		// =========================================================================================
		// KILL
		// =========================================================================================

		if (remainingMs <= 0)
		{
			KillIntruder(
				state
			);

			return;
		}


		// =========================================================================================
		// COUNTDOWN
		// =========================================================================================

		int remainingSeconds =
			(remainingMs + 999) /
			1000;


		if (
			remainingSeconds ==
			state.m_iLastIntruderSecond
		)
		{
			return;
		}


		state.m_iLastIntruderSecond =
			remainingSeconds;


		ShowMessageParam1(
			character,
			m_sEnemyWarningTitle,
			m_sEnemyWarningMessage,
			remainingSeconds,
			BPS_COUNTDOWN_HINT_DURATION
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void KillIntruder(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		IEntity character =
			state.m_Character;


		// State, NOT position math, is the authority.
		if (
			!character ||
			!state.IsInside()
		)
		{
			return;
		}


		SCR_DamageManagerComponent damageManager =
			SCR_DamageManagerComponent.GetDamageManager(
				character
			);


		if (!damageManager)
			return;


		if (damageManager.IsDestroyed())
		{
			ResetIntruder(state);

			return;
		}


		ref Instigator instigator =
			Instigator.CreateInstigatorGM();


		if (instigator)
		{
			damageManager.Kill(
				instigator
			);
		}


		if (!damageManager.IsDestroyed())
		{
			damageManager.SetHealthScaled(
				0
			);
		}


		DebugLog(
			string.Format(
				"KILL | Character=%1 | Health=%2 | Destroyed=%3",
				character,
				damageManager.GetHealthScaled(),
				damageManager.IsDestroyed()
			)
		);


		if (
			damageManager.IsDestroyed() ||
			damageManager.GetHealthScaled() <= 0
		)
		{
			ResetIntruder(state);
		}
	}


	//------------------------------------------------------------------------------------------------
	protected bool IsIntruderActive(
		BPS_CharacterState state
	)
	{
		return (
			state &&
			state.m_iIntruderStartTime >= 0
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ResetIntruder(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		state.m_iIntruderStartTime =
			-1;


		state.m_iLastIntruderSecond =
			-1;
	}


	// =============================================================================================
	// COMBAT LOCK
	// =============================================================================================

	protected void ApplyCombatLock(
		BPS_CharacterState state
	)
	{
		if (
			!state ||
			m_fCombatLockSeconds <= 0
		)
		{
			return;
		}


		bool alreadyLocked =
			IsCombatLocked(state);


		state.m_iCombatLockStartTime =
			System.GetTickCount();


		if (!alreadyLocked)
		{
			ShowMessage(
				state.m_Character,
				m_sCombatLockTitle,
				m_sCombatLockMessage
			);
		}
	}


	//------------------------------------------------------------------------------------------------
	protected bool IsCombatLocked(
		BPS_CharacterState state
	)
	{
		if (
			!state ||
			state.m_iCombatLockStartTime < 0
		)
		{
			return false;
		}


		int durationMs =
			m_fCombatLockSeconds *
			1000;


		return (
			System.GetTickCount(
				state.m_iCombatLockStartTime
			)
			<
			durationMs
		);
	}


	// =============================================================================================
	// WEAPON TRACKING
	// =============================================================================================

	protected bool HasFired(
		BPS_CharacterState state
	)
	{
		if (!state)
			return false;


		ChimeraCharacter character =
			ChimeraCharacter.Cast(
				state.m_Character
			);


		if (!character)
			return false;


		BaseWeaponComponent weapon =
			SCR_WeaponLib.GetCurrentWeaponComponent(
				character
			);


		if (!weapon)
		{
			ResetWeaponTracking(state);

			return false;
		}


		BaseMuzzleComponent muzzle =
			weapon.GetCurrentMuzzle();


		if (!muzzle)
		{
			ResetWeaponTracking(state);

			return false;
		}


		int ammo =
			muzzle.GetAmmoCount();


		if (
			state.m_Weapon != weapon ||
			state.m_Muzzle != muzzle
		)
		{
			state.m_Weapon = weapon;
			state.m_Muzzle = muzzle;

			state.m_iLastAmmo = ammo;

			state.m_bWeaponInitialized = true;

			return false;
		}


		if (!state.m_bWeaponInitialized)
		{
			state.m_iLastAmmo = ammo;

			state.m_bWeaponInitialized = true;

			return false;
		}


		bool fired =
			ammo <
			state.m_iLastAmmo;


		state.m_iLastAmmo =
			ammo;


		return fired;
	}


	//------------------------------------------------------------------------------------------------
	protected void InitializeWeaponTracking(
		BPS_CharacterState state
	)
	{
		ResetWeaponTracking(state);

		HasFired(state);
	}


	//------------------------------------------------------------------------------------------------
	protected void ResetWeaponTracking(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		state.m_Weapon = null;
		state.m_Muzzle = null;

		state.m_iLastAmmo = -1;

		state.m_bWeaponInitialized = false;
	}


	// =============================================================================================
	// DAMAGE
	// =============================================================================================

	static bool BPS_ShouldBlockDamage(
		IEntity victim,
		notnull BaseDamageContext damageContext
	)
	{
		if (!victim)
			return false;


		if (
			damageContext.damageType ==
				EDamageType.HEALING ||
			damageContext.damageType ==
				EDamageType.REGENERATION
		)
		{
			return false;
		}


		foreach (
			BPS_SafeZoneTriggerEntity zone :
			s_aZones
		)
		{
			if (
				zone &&
				zone.ShouldBlockDamage(
					victim,
					damageContext
				)
			)
			{
				return true;
			}
		}


		return false;
	}


	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockDamage(
		IEntity victim,
		notnull BaseDamageContext damageContext
	)
	{
		if (!ChimeraCharacter.Cast(victim))
			return false;


		// Only protected faction gets protection.
		if (!IsProtectedFaction(victim))
			return false;


		BPS_CharacterState victimState =
			FindState(victim);


		// =========================================================================================
		// CRITICAL FIX
		//
		// No state or state outside = BPS does NOTHING.
		//
		// An ally outside the sphere takes normal damage.
		// =========================================================================================

		if (
			!victimState ||
			!victimState.IsInside()
		)
		{
			return false;
		}


		IEntity attacker = null;


		if (damageContext.instigator)
		{
			attacker =
				damageContext.instigator.GetInstigatorEntity();
		}


		// =========================================================================================
		// FRIENDLY FIRE
		//
		// Block only when BOTH are inside.
		// =========================================================================================

		if (
			attacker &&
			attacker != victim &&
			IsProtectedFaction(attacker)
		)
		{
			BPS_CharacterState attackerState =
				FindState(attacker);


			if (
				attackerState &&
				attackerState.IsInside()
			)
			{
				ShowFriendlyFireMessage(
					attacker,
					attackerState
				);


				return true;
			}
		}


		// =========================================================================================
		// COMBAT LOCK
		//
		// Friendly-fire blocking above remains active even during Combat Lock.
		// =========================================================================================

		if (IsCombatLocked(victimState))
			return false;


		// Normal Safe Zone protection.
		return true;
	}


	// =============================================================================================
	// FRIENDLY FIRE MESSAGE
	// =============================================================================================

	protected void ShowFriendlyFireMessage(
		IEntity attacker,
		BPS_CharacterState state
	)
	{
		if (
			!attacker ||
			!state
		)
		{
			return;
		}


		if (
			state.m_iFriendlyFireMessageTime >= 0 &&
			System.GetTickCount(
				state.m_iFriendlyFireMessageTime
			)
			<
			BPS_FRIENDLY_FIRE_MESSAGE_COOLDOWN_MS
		)
		{
			return;
		}


		state.m_iFriendlyFireMessageTime =
			System.GetTickCount();


		ShowMessage(
			attacker,
			m_sFriendlyFireTitle,
			m_sFriendlyFireMessage
		);
	}


	// =============================================================================================
	// FACTION
	// =============================================================================================

	protected FactionKey GetFactionKey(
		IEntity ent
	)
	{
		if (!ent)
			return "";


		Faction faction =
			SCR_Faction.GetEntityFaction(
				ent
			);


		if (!faction)
			return "";


		return faction.GetFactionKey();
	}


	//------------------------------------------------------------------------------------------------
	protected bool IsProtectedFaction(
		IEntity ent
	)
	{
		return (
			GetFactionKey(ent) ==
			m_sProtectedFactionKey
		);
	}


	// =============================================================================================
	// STATE
	// =============================================================================================

	protected BPS_CharacterState GetOrCreateState(
		IEntity character
	)
	{
		BPS_CharacterState state =
			FindState(character);


		if (state)
			return state;


		state =
			new BPS_CharacterState(
				character
			);


		m_aStates.Insert(
			state
		);


		return state;
	}


	//------------------------------------------------------------------------------------------------
	protected BPS_CharacterState FindState(
		IEntity character
	)
	{
		if (!character)
			return null;


		foreach (
			BPS_CharacterState state :
			m_aStates
		)
		{
			if (
				state &&
				state.m_Character ==
					character
			)
			{
				return state;
			}
		}


		return null;
	}


	// =============================================================================================
	// UI
	// =============================================================================================

	protected SCR_PlayerController GetController(
		IEntity character
	)
	{
		PlayerManager playerManager =
			GetGame().GetPlayerManager();


		if (!playerManager)
			return null;


		int playerId =
			playerManager.GetPlayerIdFromControlledEntity(
				character
			);


		if (playerId <= 0)
			return null;


		return SCR_PlayerController.Cast(
			playerManager.GetPlayerController(
				playerId
			)
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ShowMessage(
		IEntity character,
		string title,
		string message
	)
	{
		if (message == "")
			return;


		SCR_PlayerController controller =
			GetController(
				character
			);


		if (!controller)
			return;


		controller.BPS_ShowMessage(
			title,
			message,
			m_fMessageDuration
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ShowMessageParam1(
		IEntity character,
		string title,
		string message,
		int param1,
		float duration
	)
	{
		SCR_PlayerController controller =
			GetController(
				character
			);


		if (!controller)
			return;


		controller.BPS_ShowMessageParam1(
			title,
			message,
			duration,
			param1
		);
	}


	// =============================================================================================
	// DEBUG
	// =============================================================================================

	protected void DebugLog(
		string message
	)
	{
		if (!m_bDebug)
			return;


		PrintFormat(
			"[BPS] %1",
			message
		);
	}


	// =============================================================================================
	// CLEANUP
	// =============================================================================================

	void ~BPS_SafeZoneTriggerEntity()
	{
		if (GetGame())
		{
			ScriptCallQueue queue =
				GetGame().GetCallqueue();


			if (queue)
			{
				queue.Remove(
					BPS_LogicTick
				);
			}
		}


		int index =
			s_aZones.Find(
				this
			);


		if (index >= 0)
		{
			s_aZones.Remove(
				index
			);
		}
	}
}