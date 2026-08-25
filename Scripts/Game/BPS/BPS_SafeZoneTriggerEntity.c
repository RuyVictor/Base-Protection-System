//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
// Safe Zone geometry: CYLINDER or SQUARE.
//
// Uses GenericEntity with explicit mathematical volume checks, so no native sphere-trigger properties are exposed.
// The real gameplay boundary is always validated by BPS_IsWorldPositionInsideShape().
// Debug visualization uses native Enfusion Shape primitives: CreateCylinder and BBOX.
// Native cylinder orientation is kept untouched (vertical in actual Reforger usage).
// Legacy compatibility helpers and obsolete vehicle-inside state have been removed.
//------------------------------------------------------------------------------------------------

enum BPS_ETriggerShapeType
{
	Cylindrical = 0,
	Square = 1
}

//------------------------------------------------------------------------------------------------
class BPS_CharacterState
{
	IEntity m_Character;

	bool m_bInside;
	bool m_bWasInside;

	int m_iIntruderStartTime;
	int m_iLastIntruderSecond;
	int m_iCombatLockStartTime;
	int m_iFriendlyFireMessageTime;

	BaseWeaponComponent m_Weapon;
	BaseMuzzleComponent m_Muzzle;
	int m_iLastAmmo;
	bool m_bWeaponInitialized;

	void BPS_CharacterState(IEntity character)
	{
		m_Character = character;
		m_bInside = false;
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

	bool IsInside()
	{
		return m_bInside;
	}
}

//------------------------------------------------------------------------------------------------
class BPS_TurretWeaponState
{
	TurretControllerComponent m_Turret;
	IEntity m_Operator;
	BaseWeaponComponent m_Weapon;
	BaseMuzzleComponent m_Muzzle;
	int m_iLastAmmo;
	bool m_bInitialized;
	bool m_bSeen;

	void BPS_TurretWeaponState(TurretControllerComponent turret)
	{
		m_Turret = turret;
		m_Operator = null;
		m_Weapon = null;
		m_Muzzle = null;
		m_iLastAmmo = -1;
		m_bInitialized = false;
		m_bSeen = false;
	}
}

//------------------------------------------------------------------------------------------------
[EntityEditorProps(
	category: "BPS/Base Protection System",
	description: "Base Protection System Safe Zone. Supports cylindrical and square volumes."
)]
class BPS_SafeZoneTriggerEntityClass : GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class BPS_SafeZoneTriggerEntity : GenericEntity
{
	protected static const int BPS_LOGIC_INTERVAL_MS = 100;
	protected static const int BPS_PRESENCE_INTERVAL_MS = 500;
	protected static const int BPS_FACTION_CHECK_INTERVAL_MS = 1000;
	// Only the 2D map outline needs polygon segmentation. The 3D debug volume uses native shapes.
	protected static const int BPS_MAP_CYLINDER_SEGMENTS = 32;

	// =============================================================================================
	// SHAPE
	// =============================================================================================
	[Attribute(
		"0",
		UIWidgets.ComboBox,
		"Trigger Shape Type",
		"",
		ParamEnumArray.FromEnum(BPS_ETriggerShapeType)
	)]
	protected BPS_ETriggerShapeType m_eTriggerShapeType;

	[Attribute(
		"200",
		UIWidgets.Slider,
		"Horizontal Size. Cylindrical = diameter. Square = side length.",
		"1 10000 1"
	)]
	protected float m_fHorizontalSize;

	[Attribute(
		"40",
		UIWidgets.Slider,
		"Safe Zone height. The volume extends half above and half below the entity origin.",
		"1 10000 1"
	)]
	protected float m_fHeight;

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show the real BPS volume in the World Editor and in-game."
	)]
	protected bool m_bShowDebugShape;

	// =============================================================================================
	// CONFIGURATION
	// =============================================================================================
	[Attribute("", UIWidgets.Object, "Friendly enter/exit UI.")]
	ref BPS_FriendlyMessagesConfig m_FriendlyMessages;

	[Attribute("", UIWidgets.Object, "Enemy intrusion configuration.")]
	ref BPS_IntruderConfig m_IntruderConfig;

	[Attribute("", UIWidgets.Object, "Combat Lock configuration.")]
	ref BPS_CombatConfig m_CombatConfig;

	[Attribute("", UIWidgets.Object, "Friendly fire configuration.")]
	ref BPS_FriendlyFireConfig m_FriendlyFireConfig;

	[Attribute("", UIWidgets.Object, "Map Safe Zone boundary configuration.")]
	ref BPS_MapDisplayConfig m_MapDisplayConfig;

	// =============================================================================================
	// CAMPAIGN / REGISTRIES
	// =============================================================================================
	protected SCR_CampaignMilitaryBaseComponent m_CampaignBase;
	protected FactionKey m_sLastBaseFactionKey;
	protected bool m_bBPSInitialized;
	protected int m_iLastPresenceRefreshTime = -1;
	protected int m_iLastFactionCheckTime = -1;

	protected static ref array<BPS_SafeZoneTriggerEntity> s_aZones = new array<BPS_SafeZoneTriggerEntity>();
	protected static ref array<BPS_SafeZoneTriggerEntity> s_aMapZones = new array<BPS_SafeZoneTriggerEntity>();

	protected ref array<ref BPS_CharacterState> m_aStates = new array<ref BPS_CharacterState>();
	protected ref array<int> m_aPlayerIds = new array<int>();
	protected ref array<ref BPS_TurretWeaponState> m_aTurretStates = new array<ref BPS_TurretWeaponState>();

	// =============================================================================================
	// INIT
	// =============================================================================================
	void BPS_SafeZoneTriggerEntity(IEntitySource src, IEntity parent)
	{
		SetFlags(EntityFlags.ACTIVE);
		// IMPORTANT: no FRAME event. Runtime debug uses one persistent Shape instead.
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		EnsureConfig();

		// Client/rendering instances get a single persistent debug shape. Dedicated
		// servers are rejected inside BPS_CreateRuntimeDebugShape().
		BPS_CreateRuntimeDebugShape();

		if (s_aMapZones.Find(this) < 0)
			s_aMapZones.Insert(this);

		if (Replication.IsRunning() && !Replication.IsServer())
		{
			if (GetGame() && GetGame().GetCallqueue())
			{
				GetGame().GetCallqueue().CallLater(
					BPS_TryBindCampaignBaseForMap,
					500,
					true
				);
			}

			return;
		}

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(
				BPS_TryInitialize,
				500,
				true
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureConfig()
	{
		if (!m_FriendlyMessages)
			m_FriendlyMessages = new BPS_FriendlyMessagesConfig();

		if (!m_IntruderConfig)
			m_IntruderConfig = new BPS_IntruderConfig();

		if (!m_CombatConfig)
			m_CombatConfig = new BPS_CombatConfig();

		if (!m_FriendlyFireConfig)
			m_FriendlyFireConfig = new BPS_FriendlyFireConfig();

		if (!m_MapDisplayConfig)
			m_MapDisplayConfig = new BPS_MapDisplayConfig();
	}

	// =============================================================================================
	// EXACT SHAPE
	// =============================================================================================
	protected float BPS_GetHorizontalSize()
	{
		if (m_fHorizontalSize < 1.0)
			return 1.0;

		return m_fHorizontalSize;
	}

	//------------------------------------------------------------------------------------------------
	protected float BPS_GetHeight()
	{
		if (m_fHeight < 1.0)
			return 1.0;

		return m_fHeight;
	}

	//------------------------------------------------------------------------------------------------
	bool BPS_IsWorldPositionInsideShape(vector worldPosition)
	{
		vector transform[4];
		GetWorldTransform(transform);

		vector localPosition = worldPosition.InvMultiply4(transform);
		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;
		float halfHeight = BPS_GetHeight() * 0.5;

		if (Math.AbsFloat(localPosition[1]) > halfHeight)
			return false;

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
		{
			return (
				Math.AbsFloat(localPosition[0]) <= halfHorizontal &&
				Math.AbsFloat(localPosition[2]) <= halfHorizontal
			);
		}

		float distanceSq =
			localPosition[0] * localPosition[0] +
			localPosition[2] * localPosition[2];

		return distanceSq <= halfHorizontal * halfHorizontal;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_IsEntityInsideShape(IEntity ent)
	{
		if (!ent)
			return false;

		return BPS_IsWorldPositionInsideShape(ent.GetOrigin());
	}

	// =============================================================================================
	// CLIENT MAP BASE BINDING
	// =============================================================================================
	protected void BPS_TryBindCampaignBaseForMap()
	{
		if (m_CampaignBase)
		{
			StopMapBindingTimer();
			return;
		}

		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!campaign)
			return;

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (!baseManager || !baseManager.IsBasesInitDone())
			return;

		m_CampaignBase = baseManager.FindClosestBase(GetOrigin());
		if (!m_CampaignBase)
			return;

		StopMapBindingTimer();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopMapBindingTimer()
	{
		if (!GetGame())
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(BPS_TryBindCampaignBaseForMap);
	}

	// =============================================================================================
	// PLAYER PRESENCE - NO WORLD QUERY
	// =============================================================================================
	// The BPS only needs to track human players for enter/exit, intrusion and combat lock.
	// Iterating PlayerManager is bounded by connected player count (for example 128) and avoids
	// QueryEntitiesBySphere scanning every nearby dynamic entity. Empty vehicle protection is
	// evaluated directly when that vehicle receives damage, so vehicles do not need polling.
	protected void BPS_RefreshPresence()
	{
		foreach (BPS_CharacterState state : m_aStates)
		{
			if (state)
			{
				state.m_bInside = false;
			}
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		m_aPlayerIds.Clear();
		playerManager.GetPlayers(m_aPlayerIds);

		foreach (int playerId : m_aPlayerIds)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
			ChimeraCharacter character = ChimeraCharacter.Cast(controlled);
			if (!character)
				continue;

			if (!BPS_IsCharacterInsideShape(character))
				continue;

			BPS_CharacterState state = GetOrCreateCharacterState(character);
			state.m_bInside = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_IsCharacterInsideShape(ChimeraCharacter character)
	{
		if (!character)
			return false;

		// On foot (and most vehicle seats), character origin is enough.
		if (BPS_IsEntityInsideShape(character))
			return true;

		// Preserve the previous BPS vehicle semantic: if the vehicle root is inside the zone,
		// its occupant is treated as inside even when the character proxy/origin is offset.
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !access.IsInCompartment())
			return false;

		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(character);
		if (!vehicle)
			return false;

		return BPS_IsEntityInsideShape(vehicle);
	}

	// =============================================================================================
	// LOGIC LOOP
	// =============================================================================================
	protected void BPS_LogicTick()
	{
		if (
			m_iLastFactionCheckTime < 0 ||
			System.GetTickCount(m_iLastFactionCheckTime) >= BPS_FACTION_CHECK_INTERVAL_MS
		)
		{
			BPS_CheckBaseFaction();
			m_iLastFactionCheckTime = System.GetTickCount();
		}

		if (
			m_iLastPresenceRefreshTime < 0 ||
			System.GetTickCount(m_iLastPresenceRefreshTime) >= BPS_PRESENCE_INTERVAL_MS
		)
		{
			BPS_RefreshPresence();
			m_iLastPresenceRefreshTime = System.GetTickCount();
		}

		foreach (BPS_TurretWeaponState turretState : m_aTurretStates)
		{
			if (turretState)
				turretState.m_bSeen = false;
		}

		ProcessCharacters();
		CleanupTurretStates();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_CheckBaseFaction()
	{
		if (!m_CampaignBase)
			return;

		Faction faction = m_CampaignBase.GetFaction();
		if (!faction)
			return;

		FactionKey currentFactionKey = faction.GetFactionKey();
		if (currentFactionKey == m_sLastBaseFactionKey)
			return;

		FactionKey previousFactionKey = m_sLastBaseFactionKey;
		m_sLastBaseFactionKey = currentFactionKey;

		PrintFormat(
			"[BPS] BASE FACTION CHANGED | Base=%1 | %2 -> %3",
			m_CampaignBase.GetBaseName(),
			previousFactionKey,
			currentFactionKey
		);

		foreach (BPS_CharacterState state : m_aStates)
		{
			if (!state || !state.IsInside())
				continue;

			ResetIntruder(state);
			state.m_iCombatLockStartTime = -1;
			state.m_bWasInside = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_TryInitialize()
	{
		if (m_bBPSInitialized)
			return;

		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!campaign)
			return;

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (!baseManager || !baseManager.IsBasesInitDone())
			return;

		m_CampaignBase = baseManager.FindClosestBase(GetOrigin());
		if (!m_CampaignBase)
		{
			Print("[BPS] Could not find Campaign base near Safe Zone.", LogLevel.ERROR);
			StopInitializationTimer();
			return;
		}

		if (!m_CampaignBase.IsHQ())
		{
			StopInitializationTimer();
			m_bBPSInitialized = true;
			return;
		}

		Faction faction = m_CampaignBase.GetFaction();
		if (!faction)
			return;

		m_sLastBaseFactionKey = faction.GetFactionKey();
		m_bBPSInitialized = true;
		StopInitializationTimer();

		if (s_aZones.Find(this) < 0)
			s_aZones.Insert(this);


		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(
				BPS_LogicTick,
				BPS_LOGIC_INTERVAL_MS,
				true
			);
		}

		PrintFormat(
			"[BPS] ACTIVATED | Base=%1 | Faction=%2 | Shape=%3 | HorizontalSize=%4 | Height=%5",
			m_CampaignBase.GetBaseName(),
			m_sLastBaseFactionKey,
			m_eTriggerShapeType,
			BPS_GetHorizontalSize(),
			BPS_GetHeight()
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopInitializationTimer()
	{
		if (!GetGame())
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(BPS_TryInitialize);
	}

	//------------------------------------------------------------------------------------------------
	protected Faction GetProtectedFaction()
	{
		if (!m_bBPSInitialized || !m_CampaignBase || !m_CampaignBase.IsHQ())
			return null;

		return m_CampaignBase.GetFaction();
	}

	// =============================================================================================
	// TURRET / MOUNTED WEAPON - CURRENT PLAYER SEAT ONLY
	// =============================================================================================
	// Instead of scanning every compartment of every vehicle in the safe zone, inspect only the
	// compartment occupied by each protected player that is currently inside.
	protected bool ProcessCharacterTurret(BPS_CharacterState characterState)
	{
		if (!characterState || !characterState.m_Character)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(characterState.m_Character);
		if (!character)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !access.IsInCompartment())
			return false;

		ExtBaseCompartmentSlot slot = ExtBaseCompartmentSlot.Cast(access.GetCompartment());
		if (!slot)
			return false;

		TurretControllerComponent turret = slot.GetAttachedTurret();
		if (!turret)
			return false;

		BPS_TurretWeaponState turretState = GetOrCreateTurretState(turret);
		turretState.m_bSeen = true;

		if (turretState.m_Operator != character)
		{
			turretState.m_Operator = character;
			ResetTurretWeaponState(turretState);
		}

		if (GetCombatDuration() > 0 && HasTurretFired(turretState))
			ApplyCombatLock(characterState);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasTurretFired(BPS_TurretWeaponState state)
	{
		if (!state || !state.m_Turret)
			return false;

		BaseWeaponManagerComponent weaponManager = state.m_Turret.GetWeaponManager();
		if (!weaponManager)
		{
			ResetTurretWeaponState(state);
			return false;
		}

		BaseWeaponComponent weapon = SCR_WeaponLib.GetCurrentWeaponComponent(weaponManager);
		if (!weapon)
		{
			ResetTurretWeaponState(state);
			return false;
		}

		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
		{
			ResetTurretWeaponState(state);
			return false;
		}

		int ammo = muzzle.GetAmmoCount();

		if (state.m_Weapon != weapon || state.m_Muzzle != muzzle)
		{
			state.m_Weapon = weapon;
			state.m_Muzzle = muzzle;
			state.m_iLastAmmo = ammo;
			state.m_bInitialized = true;
			return false;
		}

		if (!state.m_bInitialized)
		{
			state.m_iLastAmmo = ammo;
			state.m_bInitialized = true;
			return false;
		}

		bool fired = ammo < state.m_iLastAmmo;
		state.m_iLastAmmo = ammo;
		return fired;
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetTurretWeaponState(BPS_TurretWeaponState state)
	{
		if (!state)
			return;

		state.m_Weapon = null;
		state.m_Muzzle = null;
		state.m_iLastAmmo = -1;
		state.m_bInitialized = false;
	}

	//------------------------------------------------------------------------------------------------
	protected BPS_TurretWeaponState GetOrCreateTurretState(TurretControllerComponent turret)
	{
		foreach (BPS_TurretWeaponState state : m_aTurretStates)
		{
			if (state && state.m_Turret == turret)
				return state;
		}

		BPS_TurretWeaponState newState = new BPS_TurretWeaponState(turret);
		m_aTurretStates.Insert(newState);
		return newState;
	}

	//------------------------------------------------------------------------------------------------
	protected void CleanupTurretStates()
	{
		for (int i = m_aTurretStates.Count() - 1; i >= 0; i--)
		{
			BPS_TurretWeaponState state = m_aTurretStates[i];
			if (!state || !state.m_bSeen)
				m_aTurretStates.Remove(i);
		}
	}

	// =============================================================================================
	// CHARACTERS
	// =============================================================================================
	protected void ProcessCharacters()
	{
		for (int i = m_aStates.Count() - 1; i >= 0; i--)
		{
			BPS_CharacterState state = m_aStates[i];
			if (!state || !state.m_Character)
			{
				m_aStates.Remove(i);
				continue;
			}

			// Reconcile using the same character + vehicle rule used by the presence pass.
			// This avoids false exits for occupants whose character proxy sits outside the
			// volume while the vehicle itself is still inside.
			ChimeraCharacter character = ChimeraCharacter.Cast(state.m_Character);
			if (!character)
			{
				m_aStates.Remove(i);
				continue;
			}

			if (state.m_bInside && !BPS_IsCharacterInsideShape(character))
				state.m_bInside = false;

			bool inside = state.IsInside();

			if (inside && !state.m_bWasInside)
			{
				state.m_bWasInside = true;
				OnCharacterEntered(state);
			}
			else if (!inside && state.m_bWasInside)
			{
				state.m_bWasInside = false;
				OnCharacterExited(state);
			}

			if (inside && IsIntruderActive(state))
			{
				ProcessIntruder(state);
				continue;
			}

			if (inside && IsProtectedFaction(state.m_Character))
			{
				// A turret is checked only for the player's current seat. If the seat has no
				// attached turret, fall back to personal-weapon ammo tracking.
				bool hasControlledTurret = ProcessCharacterTurret(state);
				if (!hasControlledTurret && GetCombatDuration() > 0 && HasPersonalWeaponFired(state))
					ApplyCombatLock(state);

				ProcessCombatLockExpiration(state);
			}

			if (!inside && !IsIntruderActive(state) && !IsCombatLocked(state))
				m_aStates.Remove(i);
		}
	}

	// =============================================================================================
	// ENTER / EXIT
	// =============================================================================================
	protected void OnCharacterEntered(BPS_CharacterState state)
	{
		IEntity character = state.m_Character;

		if (IsProtectedFaction(character))
		{
			ResetIntruder(state);
			InitializePersonalWeaponTracking(state);

			if (m_FriendlyMessages.IsEnabled())
			{
				ShowMessage(
					character,
					m_FriendlyMessages.GetEnterTitle(),
					m_FriendlyMessages.GetEnterMessage(),
					m_FriendlyMessages.GetDuration()
				);
			}

			return;
		}

		StartIntruder(state);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCharacterExited(BPS_CharacterState state)
	{
		IEntity character = state.m_Character;
		ResetPersonalWeaponTracking(state);

		if (IsIntruderActive(state))
		{
			ResetIntruder(state);
			ShowMessage(
				character,
				m_IntruderConfig.GetExitTitle(),
				m_IntruderConfig.GetExitMessage(),
				m_IntruderConfig.GetExitMessageDuration()
			);
			return;
		}

		if (IsProtectedFaction(character) && m_FriendlyMessages.IsEnabled())
		{
			ShowMessage(
				character,
				m_FriendlyMessages.GetExitTitle(),
				m_FriendlyMessages.GetExitMessage(),
				m_FriendlyMessages.GetDuration()
			);
		}
	}

	// =============================================================================================
	// PERSONAL WEAPON / COMBAT LOCK
	// =============================================================================================
	protected bool HasPersonalWeaponFired(BPS_CharacterState state)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(state.m_Character);
		if (!character)
			return false;

		BaseWeaponComponent weapon = SCR_WeaponLib.GetCurrentWeaponComponent(character);
		if (!weapon)
		{
			ResetPersonalWeaponTracking(state);
			return false;
		}

		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
		{
			ResetPersonalWeaponTracking(state);
			return false;
		}

		int ammo = muzzle.GetAmmoCount();

		if (state.m_Weapon != weapon || state.m_Muzzle != muzzle)
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

		bool fired = ammo < state.m_iLastAmmo;
		state.m_iLastAmmo = ammo;
		return fired;
	}

	//------------------------------------------------------------------------------------------------
	protected void InitializePersonalWeaponTracking(BPS_CharacterState state)
	{
		ResetPersonalWeaponTracking(state);
		HasPersonalWeaponFired(state);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetPersonalWeaponTracking(BPS_CharacterState state)
	{
		state.m_Weapon = null;
		state.m_Muzzle = null;
		state.m_iLastAmmo = -1;
		state.m_bWeaponInitialized = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCombatLock(BPS_CharacterState state)
	{
		if (!state || GetCombatDuration() <= 0)
			return;

		bool alreadyLocked = IsCombatLocked(state);
		state.m_iCombatLockStartTime = System.GetTickCount();

		if (!alreadyLocked)
		{
			ShowMessage(
				state.m_Character,
				m_CombatConfig.GetLockTitle(),
				m_CombatConfig.GetLockMessage(),
				m_CombatConfig.GetMessageDuration()
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCombatLocked(BPS_CharacterState state)
	{
		if (!state || state.m_iCombatLockStartTime < 0)
			return false;

		int durationMs = GetCombatDuration() * 1000;
		return System.GetTickCount(state.m_iCombatLockStartTime) < durationMs;
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessCombatLockExpiration(BPS_CharacterState state)
	{
		if (state.m_iCombatLockStartTime < 0 || IsCombatLocked(state))
			return;

		state.m_iCombatLockStartTime = -1;

		ShowMessage(
			state.m_Character,
			m_CombatConfig.GetRestoredTitle(),
			m_CombatConfig.GetRestoredMessage(),
			m_CombatConfig.GetMessageDuration()
		);
	}

	// =============================================================================================
	// INTRUDER
	// =============================================================================================
	protected void StartIntruder(BPS_CharacterState state)
	{
		state.m_iIntruderStartTime = System.GetTickCount();
		state.m_iLastIntruderSecond = GetIntruderDelay();

		ShowMessageParam1(
			state.m_Character,
			m_IntruderConfig.GetWarningTitle(),
			m_IntruderConfig.GetWarningMessage(),
			m_IntruderConfig.GetCountdownMessageDuration(),
			GetIntruderDelay()
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessIntruder(BPS_CharacterState state)
	{
		if (!state.IsInside())
			return;

		if (IsProtectedFaction(state.m_Character))
		{
			ResetIntruder(state);
			return;
		}

		int elapsed = System.GetTickCount(state.m_iIntruderStartTime);
		int total = GetIntruderDelay() * 1000;
		int remainingMs = total - elapsed;

		if (remainingMs <= 0)
		{
			KillIntruder(state);
			return;
		}

		int remaining = (remainingMs + 999) / 1000;
		if (remaining == state.m_iLastIntruderSecond)
			return;

		state.m_iLastIntruderSecond = remaining;

		ShowMessageParam1(
			state.m_Character,
			m_IntruderConfig.GetWarningTitle(),
			m_IntruderConfig.GetWarningMessage(),
			m_IntruderConfig.GetCountdownMessageDuration(),
			remaining
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void KillIntruder(BPS_CharacterState state)
	{
		if (!state || !state.IsInside())
			return;

		SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.GetDamageManager(state.m_Character);
		if (!manager)
			return;

		if (manager.IsDestroyed())
		{
			ResetIntruder(state);
			return;
		}

		ref Instigator instigator = Instigator.CreateInstigatorGM();
		if (instigator)
			manager.Kill(instigator);

		if (!manager.IsDestroyed())
			manager.SetHealthScaled(0);

		if (manager.IsDestroyed() || manager.GetHealthScaled() <= 0)
			ResetIntruder(state);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsIntruderActive(BPS_CharacterState state)
	{
		return state && state.m_iIntruderStartTime >= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetIntruder(BPS_CharacterState state)
	{
		if (!state)
			return;

		state.m_iIntruderStartTime = -1;
		state.m_iLastIntruderSecond = -1;
	}

	// =============================================================================================
	// DAMAGE
	// =============================================================================================
	static bool BPS_ShouldBlockDamage(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (
			damageContext.damageType == EDamageType.HEALING ||
			damageContext.damageType == EDamageType.REGENERATION
		)
		{
			return false;
		}

		foreach (BPS_SafeZoneTriggerEntity zone : s_aZones)
		{
			if (zone && zone.ShouldBlockDamage(victim, damageContext))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockDamage(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (!victim)
			return false;

		if (ChimeraCharacter.Cast(victim))
			return ShouldBlockCharacterDamage(victim, damageContext);

		if (Vehicle.Cast(victim))
			return ShouldBlockVehicleDamage(victim, damageContext);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockCharacterDamage(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (!IsProtectedFaction(victim))
			return false;

		BPS_CharacterState victimState = FindCharacterState(victim);
		if (!victimState || !victimState.IsInside())
			return false;

		IEntity attacker = GetDamageInstigatorEntity(damageContext);

		if (
			m_FriendlyFireConfig.IsEnabled() &&
			attacker &&
			attacker != victim &&
			IsProtectedFaction(attacker)
		)
		{
			BPS_CharacterState attackerState = FindCharacterState(attacker);
			if (attackerState && attackerState.IsInside())
				ShowFriendlyFireMessage(attacker, attackerState);

			return true;
		}

		if (IsCombatLocked(victimState))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockVehicleDamage(IEntity vehicle, notnull BaseDamageContext damageContext)
	{
		if (!m_FriendlyFireConfig.IsEnabled())
			return false;

		if (!BPS_IsEntityInsideShape(vehicle))
			return false;

		if (!IsProtectedFaction(vehicle))
			return false;

		IEntity attacker = GetDamageInstigatorEntity(damageContext);
		if (!attacker || attacker == vehicle)
			return false;

		if (!IsProtectedFaction(attacker))
			return false;

		ChimeraCharacter attackerCharacter = ChimeraCharacter.Cast(attacker);
		if (attackerCharacter)
		{
			BPS_CharacterState attackerState = FindCharacterState(attackerCharacter);
			if (attackerState && attackerState.IsInside())
				ShowFriendlyFireMessage(attackerCharacter, attackerState);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity GetDamageInstigatorEntity(notnull BaseDamageContext damageContext)
	{
		if (!damageContext.instigator)
			return null;

		return damageContext.instigator.GetInstigatorEntity();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowFriendlyFireMessage(IEntity attacker, BPS_CharacterState state)
	{
		int cooldown = m_FriendlyFireConfig.GetWarningCooldown() * 1000;

		if (
			state.m_iFriendlyFireMessageTime >= 0 &&
			System.GetTickCount(state.m_iFriendlyFireMessageTime) < cooldown
		)
		{
			return;
		}

		state.m_iFriendlyFireMessageTime = System.GetTickCount();

		ShowMessage(
			attacker,
			m_FriendlyFireConfig.GetTitle(),
			m_FriendlyFireConfig.GetMessage(),
			m_FriendlyFireConfig.GetMessageDuration()
		);
	}

	// =============================================================================================
	// FACTION HELPERS
	// =============================================================================================
	protected Faction GetEntityFactionForBPS(IEntity ent)
	{
		if (!ent)
			return null;

		Faction faction = SCR_Faction.GetEntityFaction(ent);
		if (faction)
			return faction;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(
			ent.FindComponent(FactionAffiliationComponent)
		);

		if (!affiliation)
			return null;

		faction = affiliation.GetAffiliatedFaction();
		if (faction)
			return faction;

		return affiliation.GetDefaultAffiliatedFaction();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsProtectedFaction(IEntity ent)
	{
		if (!ent)
			return false;

		Faction protectedFaction = GetProtectedFaction();
		if (!protectedFaction)
			return false;

		Faction entityFaction = GetEntityFactionForBPS(ent);
		if (!entityFaction)
			return false;

		return entityFaction.GetFactionKey() == protectedFaction.GetFactionKey();
	}

	// =============================================================================================
	// MAP API
	// =============================================================================================
	static void BPS_CopyMapZones(notnull array<BPS_SafeZoneTriggerEntity> zones)
	{
		zones.Clear();

		foreach (BPS_SafeZoneTriggerEntity zone : s_aMapZones)
		{
			if (zone)
				zones.Insert(zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	bool BPS_ShouldShowMapBoundary()
	{
		if (!m_MapDisplayConfig || !m_MapDisplayConfig.IsEnabled())
			return false;

		if (!m_CampaignBase)
			return false;

		return m_CampaignBase.IsHQ();
	}

	//------------------------------------------------------------------------------------------------
	void BPS_BuildMapWorldOutline(notnull array<vector> points)
	{
		points.Clear();

		vector transform[4];
		GetWorldTransform(transform);

		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
		{
			points.Insert(Vector(-halfHorizontal, 0, -halfHorizontal).Multiply4(transform));
			points.Insert(Vector( halfHorizontal, 0, -halfHorizontal).Multiply4(transform));
			points.Insert(Vector( halfHorizontal, 0,  halfHorizontal).Multiply4(transform));
			points.Insert(Vector(-halfHorizontal, 0,  halfHorizontal).Multiply4(transform));
			return;
		}

		for (int i = 0; i < BPS_MAP_CYLINDER_SEGMENTS; i++)
		{
			float angle = (Math.PI2 * i) / BPS_MAP_CYLINDER_SEGMENTS;
			vector localPoint = Vector(
				Math.Cos(angle) * halfHorizontal,
				0,
				Math.Sin(angle) * halfHorizontal
			);

			points.Insert(localPoint.Multiply4(transform));
		}
	}

	//------------------------------------------------------------------------------------------------
	float BPS_GetMapBorderSize()
	{
		return m_MapDisplayConfig.GetBorderSize();
	}

	//------------------------------------------------------------------------------------------------
	bool BPS_IsEnemyMapZoneForLocalPlayer()
	{
		if (!m_CampaignBase)
			return false;

		Faction zoneFaction = m_CampaignBase.GetFaction();
		if (!zoneFaction)
			return false;

		Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
		if (!localFaction)
		{
			int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
			if (localPlayerId > 0)
				localFaction = SCR_FactionManager.SGetPlayerFaction(localPlayerId);
		}

		if (!localFaction)
			return false;

		return localFaction.GetFactionKey() != zoneFaction.GetFactionKey();
	}

	//------------------------------------------------------------------------------------------------
	Color BPS_GetMapBorderColor()
	{
		if (BPS_IsEnemyMapZoneForLocalPlayer())
			return m_MapDisplayConfig.GetEnemyBorderColor();

		return m_MapDisplayConfig.GetFriendlyBorderColor();
	}

	//------------------------------------------------------------------------------------------------
	Color BPS_GetMapBackgroundColor()
	{
		if (BPS_IsEnemyMapZoneForLocalPlayer())
			return m_MapDisplayConfig.GetEnemyBackgroundColor();

		return m_MapDisplayConfig.GetFriendlyBackgroundColor();
	}

	// =============================================================================================
	// CONFIG HELPERS
	// =============================================================================================
	protected int GetIntruderDelay()
	{
		return m_IntruderConfig.GetKillDelaySeconds();
	}

	//------------------------------------------------------------------------------------------------
	protected float GetCombatDuration()
	{
		return m_CombatConfig.GetDuration();
	}

	// =============================================================================================
	// CHARACTER STATE
	// =============================================================================================
	protected BPS_CharacterState GetOrCreateCharacterState(IEntity character)
	{
		BPS_CharacterState existing = FindCharacterState(character);
		if (existing)
			return existing;

		BPS_CharacterState state = new BPS_CharacterState(character);
		m_aStates.Insert(state);
		return state;
	}

	//------------------------------------------------------------------------------------------------
	protected BPS_CharacterState FindCharacterState(IEntity character)
	{
		foreach (BPS_CharacterState state : m_aStates)
		{
			if (state && state.m_Character == character)
				return state;
		}

		return null;
	}

	// =============================================================================================
	// UI
	// =============================================================================================
	protected SCR_PlayerController GetController(IEntity character)
	{
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return null;

		int playerId = manager.GetPlayerIdFromControlledEntity(character);
		if (playerId <= 0)
			return null;

		return SCR_PlayerController.Cast(manager.GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowMessage(IEntity character, string title, string message, float duration)
	{
		if (message == "")
			return;

		SCR_PlayerController controller = GetController(character);
		if (!controller)
			return;

		controller.BPS_ShowMessage(title, message, duration);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowMessageParam1(
		IEntity character,
		string title,
		string message,
		float duration,
		int param1
	)
	{
		SCR_PlayerController controller = GetController(character);
		if (!controller)
			return;

		controller.BPS_ShowMessageParam1(title, message, duration, param1);
	}

	// =============================================================================================
	// DEBUG VOLUME - NATIVE ENFUSION SHAPES
	// =============================================================================================
	// Cylindrical uses Shape.CreateCylinder(). Square uses Shape.Create(ShapeType.BBOX).
	// No manual 3D segment/triangle generation is performed by BPS.
	//
	// Both runtime and Workbench keep ONE persistent native Shape. Workbench only recreates it
	// when a size/type/debug property changes; movement/rotation only updates SetMatrix().
	protected ref Shape m_BPSRuntimeDebugShape;
#ifdef WORKBENCH
	protected ref Shape m_BPSWorkbenchDebugShape;
#endif

	//------------------------------------------------------------------------------------------------
	protected int BPS_GetDebugColor()
	{
		// Native primitives contain solid faces. Keep alpha low and use NOOUTLINE so the engine's
		// internal cylinder triangle mesh is not shown as visible triangle edges.
		return Color.FromRGBA(0, 255, 70, 55).PackToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected ShapeFlags BPS_GetDebugShapeFlags()
	{
		// Matches the native Reforger debug-volume approach: persistent, translucent,
		// double-sided, additive and without mesh outlines.
		return
			ShapeFlags.VISIBLE |
			ShapeFlags.NOZBUFFER |
			ShapeFlags.TRANSP |
			ShapeFlags.NOOUTLINE |
			ShapeFlags.ADDITIVE |
			ShapeFlags.DOUBLESIDE;
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_UpdateNativeDebugShapeTransform(Shape shape)
	{
		if (!shape)
			return;

		vector transform[4];

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Cylindrical)
		{
			// IMPORTANT:
			// In actual Arma Reforger usage Shape.CreateCylinder is already used as a
			// vertical (world-Y) primitive. Applying the previous manual axis remap
			// rotated the native cylinder onto its side.
			//
			// Keep the native orientation and update translation only. A cylinder is
			// rotationally symmetric around its vertical axis, so entity yaw is irrelevant.
			Math3D.MatrixIdentity4(transform);
			transform[3] = GetOrigin();
		}
		else
		{
			// Square safe zones must preserve entity orientation (especially yaw).
			GetWorldTransform(transform);
		}

		shape.SetMatrix(transform);
	}

	//------------------------------------------------------------------------------------------------
	protected Shape BPS_CreateNativeCylinderDebugShape()
	{
		Shape shape = Shape.CreateCylinder(
			BPS_GetDebugColor(),
			BPS_GetDebugShapeFlags(),
			vector.Zero,
			BPS_GetHorizontalSize() * 0.5,
			BPS_GetHeight()
		);

		if (!shape)
			return null;

		BPS_UpdateNativeDebugShapeTransform(shape);
		return shape;
	}

	//------------------------------------------------------------------------------------------------
	protected Shape BPS_CreateNativeBoxDebugShape()
	{
		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;
		float halfHeight = BPS_GetHeight() * 0.5;

		Shape shape = Shape.Create(
			ShapeType.BBOX,
			BPS_GetDebugColor(),
			BPS_GetDebugShapeFlags(),
			Vector(-halfHorizontal, -halfHeight, -halfHorizontal),
			Vector( halfHorizontal,  halfHeight,  halfHorizontal)
		);

		if (!shape)
			return null;

		BPS_UpdateNativeDebugShapeTransform(shape);
		return shape;
	}

	//------------------------------------------------------------------------------------------------
	protected Shape BPS_CreateNativeDebugShape()
	{
		if (!m_bShowDebugShape)
			return null;

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
			return BPS_CreateNativeBoxDebugShape();

		return BPS_CreateNativeCylinderDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_CreateRuntimeDebugShape()
	{
		BPS_DestroyRuntimeDebugShape();

		if (!m_bShowDebugShape)
			return;

		// Dedicated/console applications have no renderer. There is no FRAME event in this entity,
		// so debug visualization contributes no per-frame callback on the dedicated server.
		if (System.IsConsoleApp())
			return;

#ifdef WORKBENCH
		// Edit mode owns a separate Workbench preview Shape.
		if (!GetGame() || !GetGame().InPlayMode())
			return;
#endif

		m_BPSRuntimeDebugShape = BPS_CreateNativeDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_DestroyRuntimeDebugShape()
	{
		if (!m_BPSRuntimeDebugShape)
			return;

		delete m_BPSRuntimeDebugShape;
		m_BPSRuntimeDebugShape = null;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	protected void BPS_DestroyWorkbenchDebugShape()
	{
		if (!m_BPSWorkbenchDebugShape)
			return;

		delete m_BPSWorkbenchDebugShape;
		m_BPSWorkbenchDebugShape = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_RebuildWorkbenchDebugShape()
	{
		BPS_DestroyWorkbenchDebugShape();

		if (!m_bShowDebugShape)
			return;

		if (GetGame() && GetGame().InPlayMode())
			return;

		m_BPSWorkbenchDebugShape = BPS_CreateNativeDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_OnInit(inout vector mat[4], IEntitySource src)
	{
		super._WB_OnInit(mat, src);

		if (src)
			BPS_WBSyncChangedProperty(src, "m_eTriggerShapeType");
		if (src)
			BPS_WBSyncChangedProperty(src, "m_fHorizontalSize");
		if (src)
			BPS_WBSyncChangedProperty(src, "m_fHeight");
		if (src)
			BPS_WBSyncChangedProperty(src, "m_bShowDebugShape");

		BPS_RebuildWorkbenchDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	override bool _WB_OnKeyChanged(BaseContainer src, string key, BaseContainerList ownerContainers, IEntity parent)
	{
		// Rebuild the native debug primitive only when one of its own properties changed.
		// Editing messages, colors or other BPS configuration no longer reallocates the Shape.
		if (BPS_WBSyncChangedProperty(src, key))
		{
			if (GetGame() && GetGame().InPlayMode())
				BPS_CreateRuntimeDebugShape();
			else
				BPS_RebuildWorkbenchDebugShape();
		}

		return super._WB_OnKeyChanged(src, key, ownerContainers, parent);
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_WBSyncChangedProperty(BaseContainer src, string key)
	{
		if (!src)
			return false;

		if (key == "m_eTriggerShapeType")
		{
			int shapeTypeValue;
			if (!src.Get(key, shapeTypeValue))
				return false;

			if (shapeTypeValue == BPS_ETriggerShapeType.Square)
				m_eTriggerShapeType = BPS_ETriggerShapeType.Square;
			else
				m_eTriggerShapeType = BPS_ETriggerShapeType.Cylindrical;

			return true;
		}

		if (key == "m_fHorizontalSize")
		{
			float horizontalSizeValue;
			if (!src.Get(key, horizontalSizeValue))
				return false;

			m_fHorizontalSize = horizontalSizeValue;
			return true;
		}

		if (key == "m_fHeight")
		{
			float heightValue;
			if (!src.Get(key, heightValue))
				return false;

			m_fHeight = heightValue;
			return true;
		}

		if (key == "m_bShowDebugShape")
		{
			bool showDebugValue;
			if (!src.Get(key, showDebugValue))
				return false;

			m_bShowDebugShape = showDebugValue;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		if (GetGame() && GetGame().InPlayMode())
			return;

		if (!m_bShowDebugShape)
		{
			BPS_DestroyWorkbenchDebugShape();
			return;
		}

		if (!m_BPSWorkbenchDebugShape)
		{
			BPS_RebuildWorkbenchDebugShape();
			return;
		}

		// Moving/rotating the entity requires only a matrix update; no Shape allocation.
		BPS_UpdateNativeDebugShapeTransform(m_BPSWorkbenchDebugShape);
	}
#endif

	// =============================================================================================
	// CLEANUP
	// =============================================================================================
	void ~BPS_SafeZoneTriggerEntity()
	{
		BPS_DestroyRuntimeDebugShape();
#ifdef WORKBENCH
		BPS_DestroyWorkbenchDebugShape();
#endif

		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(BPS_LogicTick);
				queue.Remove(BPS_TryBindCampaignBaseForMap);
				queue.Remove(BPS_TryInitialize);
			}
		}

		int mapIndex = s_aMapZones.Find(this);
		if (mapIndex >= 0)
			s_aMapZones.Remove(mapIndex);

		int index = s_aZones.Find(this);
		if (index >= 0)
			s_aZones.Remove(index);
	}
}
