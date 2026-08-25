//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
// Safe Zone geometry: CYLINDER or SQUARE.
//
// Uses GenericEntity + an internal world query, so no native sphere-trigger properties are exposed.
// The real gameplay boundary is always validated by BPS_IsWorldPositionInsideShape().
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

	bool m_bDirectInside;
	bool m_bVehicleInside;
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

	bool IsInside()
	{
		return m_bDirectInside || m_bVehicleInside;
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
	protected static const int BPS_PRESENCE_INTERVAL_MS = 250;
	protected static const int BPS_DEBUG_CYLINDER_SEGMENTS = 48;
	protected static const int BPS_MAP_CYLINDER_SEGMENTS = 64;

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
	protected BPS_ETriggerShapeType m_eTriggerShapeType = BPS_ETriggerShapeType.Cylindrical;

	[Attribute(
		"200",
		UIWidgets.Slider,
		"Horizontal Size. Cylindrical = diameter. Square = side length.",
		"1 5000 1"
	)]
	protected float m_fHorizontalSize = 200.0;

	[Attribute(
		"40",
		UIWidgets.Slider,
		"Safe Zone height. The volume extends half above and half below the entity origin.",
		"1 1000 1"
	)]
	protected float m_fHeight = 40.0;

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show the real BPS volume in the World Editor and in-game."
	)]
	protected bool m_bShowDebugShape = true;

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

	protected static ref array<BPS_SafeZoneTriggerEntity> s_aZones = new array<BPS_SafeZoneTriggerEntity>();
	protected static ref array<BPS_SafeZoneTriggerEntity> s_aMapZones = new array<BPS_SafeZoneTriggerEntity>();

	protected ref array<ref BPS_CharacterState> m_aStates = new array<ref BPS_CharacterState>();
	protected ref array<IEntity> m_aVehiclesInside = new array<IEntity>();
	protected ref array<IEntity> m_aOccupants = new array<IEntity>();
	protected ref array<BaseCompartmentSlot> m_aCompartments = new array<BaseCompartmentSlot>();
	protected ref array<ref BPS_TurretWeaponState> m_aTurretStates = new array<ref BPS_TurretWeaponState>();

	// =============================================================================================
	// INIT
	// =============================================================================================
	void BPS_SafeZoneTriggerEntity(IEntitySource src, IEntity parent)
	{
		SetFlags(EntityFlags.ACTIVE);
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// ShapeFlags.ONCE only persists for the current rendered frame, so the debug
		// volume must be recreated every frame while runtime debug is enabled.
		// Dedicated servers have no local renderer and must not spend time drawing it.
		if (!m_bShowDebugShape || RplSession.Mode() == RplMode.Dedicated)
			return;

		BPS_DrawDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		EnsureConfig();

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
	float BPS_GetShapeRadius()
	{
		return BPS_GetHorizontalSize() * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	float BPS_GetShapeHeight()
	{
		return BPS_GetHeight();
	}

	//------------------------------------------------------------------------------------------------
	BPS_ETriggerShapeType BPS_GetShapeType()
	{
		return m_eTriggerShapeType;
	}

	//------------------------------------------------------------------------------------------------
	protected float BPS_GetBroadPhaseRadius()
	{
		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;
		float halfHeight = BPS_GetHeight() * 0.5;
		float horizontalExtent = halfHorizontal;

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
			horizontalExtent = halfHorizontal * Math.Sqrt(2.0);

		return Math.Sqrt(horizontalExtent * horizontalExtent + halfHeight * halfHeight) + 0.5;
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
	// PRESENCE QUERY
	// =============================================================================================
	// GenericEntity is used intentionally so the prefab does not expose native sphere trigger
	// properties. A private world query gathers candidates; BPS then performs the exact shape test.
	protected void BPS_RefreshPresence()
	{
		foreach (BPS_CharacterState state : m_aStates)
		{
			if (state)
			{
				state.m_bDirectInside = false;
				state.m_bVehicleInside = false;
			}
		}

		m_aVehiclesInside.Clear();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		world.QueryEntitiesBySphere(
			GetOrigin(),
			BPS_GetBroadPhaseRadius(),
			BPS_OnQueryEntity,
			BPS_FilterQueryEntity,
			EQueryEntitiesFlags.ALL
		);
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_FilterQueryEntity(IEntity ent)
	{
		if (!ent || ent == this)
			return false;

		if (ChimeraCharacter.Cast(ent))
			return true;

		return GetCompartmentManager(ent) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_OnQueryEntity(IEntity ent)
	{
		if (!ent || !BPS_IsEntityInsideShape(ent))
			return true;

		ChimeraCharacter character = ChimeraCharacter.Cast(ent);
		if (character)
		{
			BPS_CharacterState state = GetOrCreateCharacterState(character);
			state.m_bDirectInside = true;
			return true;
		}

		if (GetCompartmentManager(ent) && m_aVehiclesInside.Find(ent) < 0)
			m_aVehiclesInside.Insert(ent);

		return true;
	}

	// =============================================================================================
	// LOGIC LOOP
	// =============================================================================================
	protected void BPS_LogicTick()
	{
		BPS_CheckBaseFaction();

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

		ProcessVehicles();
		CleanupTurretStates();
		ProcessCharacters();
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
	// VEHICLES
	// =============================================================================================
	protected void ProcessVehicles()
	{
		for (int i = m_aVehiclesInside.Count() - 1; i >= 0; i--)
		{
			IEntity vehicle = m_aVehiclesInside[i];
			if (!vehicle || !BPS_IsEntityInsideShape(vehicle))
			{
				m_aVehiclesInside.Remove(i);
				continue;
			}

			SCR_BaseCompartmentManagerComponent manager = GetCompartmentManager(vehicle);
			if (!manager)
			{
				m_aVehiclesInside.Remove(i);
				continue;
			}

			ProcessVehicleOccupants(manager);
			ProcessVehicleTurrets(manager);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessVehicleOccupants(SCR_BaseCompartmentManagerComponent manager)
	{
		m_aOccupants.Clear();
		manager.GetOccupants(m_aOccupants);

		foreach (IEntity occupant : m_aOccupants)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(occupant);
			if (!character)
				continue;

			BPS_CharacterState state = GetOrCreateCharacterState(character);
			state.m_bVehicleInside = true;
		}
	}

	// =============================================================================================
	// TURRETS / MOUNTED WEAPONS
	// =============================================================================================
	protected void ProcessVehicleTurrets(SCR_BaseCompartmentManagerComponent manager)
	{
		m_aCompartments.Clear();
		manager.GetCompartments(m_aCompartments);

		foreach (BaseCompartmentSlot baseSlot : m_aCompartments)
		{
			ExtBaseCompartmentSlot slot = ExtBaseCompartmentSlot.Cast(baseSlot);
			if (!slot)
				continue;

			TurretControllerComponent turret = slot.GetAttachedTurret();
			if (!turret)
				continue;

			BPS_TurretWeaponState turretState = GetOrCreateTurretState(turret);
			turretState.m_bSeen = true;

			IEntity operator = slot.GetOccupant();
			if (!operator || !ChimeraCharacter.Cast(operator))
			{
				ResetTurretWeaponState(turretState);
				continue;
			}

			if (turretState.m_Operator != operator)
			{
				turretState.m_Operator = operator;
				ResetTurretWeaponState(turretState);
			}

			if (!IsProtectedFaction(operator))
				continue;

			BPS_CharacterState operatorState = GetOrCreateCharacterState(operator);
			if (!operatorState.IsInside())
				continue;

			if (HasTurretFired(turretState))
				ApplyCombatLock(operatorState);
		}
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

			// Safety reconciliation for direct characters. This makes shape changes and
			// edge crossings deterministic even if native trigger callbacks are delayed.
			if (state.m_bDirectInside && !BPS_IsEntityInsideShape(state.m_Character))
				state.m_bDirectInside = false;

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
				if (GetCombatDuration() > 0 && HasPersonalWeaponFired(state))
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

		if (GetCompartmentManager(victim))
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

		if (m_aVehiclesInside.Find(vehicle) < 0 || !BPS_IsEntityInsideShape(vehicle))
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
	// VEHICLE / FACTION HELPERS
	// =============================================================================================
	protected SCR_BaseCompartmentManagerComponent GetCompartmentManager(IEntity ent)
	{
		if (!ent)
			return null;

		return SCR_BaseCompartmentManagerComponent.Cast(
			ent.FindComponent(SCR_BaseCompartmentManagerComponent)
		);
	}

	//------------------------------------------------------------------------------------------------
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
	vector BPS_GetMapWorldPosition()
	{
		return GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	float BPS_GetMapRadius()
	{
		// Compatibility helper. This is the real cylinder radius / square half-side.
		return BPS_GetHorizontalSize() * 0.5;
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
	// DEBUG VOLUME - WORLD EDITOR
	// =============================================================================================
#ifdef WORKBENCH
	// Workbench edits the IEntitySource/BaseContainer first. The live script instance used by
	// _WB_AfterWorldUpdate is not guaranteed to receive those Attribute values immediately.
	// Keep the editor source so the preview can synchronize every editor frame while a slider
	// is being dragged, and also synchronize immediately from _WB_OnKeyChanged.
	protected IEntitySource m_BPSWorkbenchSource;

	//------------------------------------------------------------------------------------------------
	override void _WB_OnInit(inout vector mat[4], IEntitySource src)
	{
		super._WB_OnInit(mat, src);

		m_BPSWorkbenchSource = src;
		BPS_WBSyncShapeProperties(src);
	}

	//------------------------------------------------------------------------------------------------
	override bool _WB_OnKeyChanged(BaseContainer src, string key, BaseContainerList ownerContainers, IEntity parent)
	{
		// Sync before the next draw. This fires for ComboBox changes and for slider edits.
		// Using the source values is important because the live entity instance may still hold
		// the old serialized Attribute values at this point.
		BPS_WBSyncShapeProperties(src);

		return super._WB_OnKeyChanged(src, key, ownerContainers, parent);
	}

	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		// Keep the current source in case the editor recreated/reloaded the entity source.
		m_BPSWorkbenchSource = src;

		// Draw while visible. ONCE debug shapes are recreated each editor frame.
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		// Critical for real-time sliders: read the current editor container every frame.
		// This also covers cases where Workbench updates a slider value continuously without
		// immediately propagating the serialized Attribute into this live script instance.
		if (m_BPSWorkbenchSource)
			BPS_WBSyncShapeProperties(m_BPSWorkbenchSource);

		if (!m_bShowDebugShape)
			return;

		BPS_DrawDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_WBSyncShapeProperties(BaseContainer src)
	{
		if (!src)
			return;

		int shapeTypeValue;
		if (src.Get("m_eTriggerShapeType", shapeTypeValue))
		{
			if (shapeTypeValue == BPS_ETriggerShapeType.Square)
				m_eTriggerShapeType = BPS_ETriggerShapeType.Square;
			else
				m_eTriggerShapeType = BPS_ETriggerShapeType.Cylindrical;
		}

		float horizontalSizeValue;
		if (src.Get("m_fHorizontalSize", horizontalSizeValue))
			m_fHorizontalSize = horizontalSizeValue;

		float heightValue;
		if (src.Get("m_fHeight", heightValue))
			m_fHeight = heightValue;

		bool showDebugValue;
		if (src.Get("m_bShowDebugShape", showDebugValue))
			m_bShowDebugShape = showDebugValue;
	}
#endif

	//------------------------------------------------------------------------------------------------
	protected void BPS_DrawDebugLine(vector from, vector to, int color, ShapeFlags flags)
	{
		vector points[2];
		points[0] = from;
		points[1] = to;
		Shape.CreateLines(color, flags, points, 2);
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_DrawDebugShape()
	{
		vector transform[4];
		GetWorldTransform(transform);

		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;
		float halfHeight = BPS_GetHeight() * 0.5;

		Color debugColor = Color.FromRGBA(0, 255, 70, 220);
		int color = debugColor.PackToInt();
		ShapeFlags flags = ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP;

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
		{
			vector corners[8];
			corners[0] = Vector(-halfHorizontal, -halfHeight, -halfHorizontal).Multiply4(transform);
			corners[1] = Vector( halfHorizontal, -halfHeight, -halfHorizontal).Multiply4(transform);
			corners[2] = Vector( halfHorizontal, -halfHeight,  halfHorizontal).Multiply4(transform);
			corners[3] = Vector(-halfHorizontal, -halfHeight,  halfHorizontal).Multiply4(transform);
			corners[4] = Vector(-halfHorizontal,  halfHeight, -halfHorizontal).Multiply4(transform);
			corners[5] = Vector( halfHorizontal,  halfHeight, -halfHorizontal).Multiply4(transform);
			corners[6] = Vector( halfHorizontal,  halfHeight,  halfHorizontal).Multiply4(transform);
			corners[7] = Vector(-halfHorizontal,  halfHeight,  halfHorizontal).Multiply4(transform);

			BPS_DrawDebugLine(corners[0], corners[1], color, flags);
			BPS_DrawDebugLine(corners[1], corners[2], color, flags);
			BPS_DrawDebugLine(corners[2], corners[3], color, flags);
			BPS_DrawDebugLine(corners[3], corners[0], color, flags);
			BPS_DrawDebugLine(corners[4], corners[5], color, flags);
			BPS_DrawDebugLine(corners[5], corners[6], color, flags);
			BPS_DrawDebugLine(corners[6], corners[7], color, flags);
			BPS_DrawDebugLine(corners[7], corners[4], color, flags);
			BPS_DrawDebugLine(corners[0], corners[4], color, flags);
			BPS_DrawDebugLine(corners[1], corners[5], color, flags);
			BPS_DrawDebugLine(corners[2], corners[6], color, flags);
			BPS_DrawDebugLine(corners[3], corners[7], color, flags);
			return;
		}

		for (int i = 0; i < BPS_DEBUG_CYLINDER_SEGMENTS; i++)
		{
			int next = (i + 1) % BPS_DEBUG_CYLINDER_SEGMENTS;
			float angleA = (Math.PI2 * i) / BPS_DEBUG_CYLINDER_SEGMENTS;
			float angleB = (Math.PI2 * next) / BPS_DEBUG_CYLINDER_SEGMENTS;

			vector bottomA = Vector(Math.Cos(angleA) * halfHorizontal, -halfHeight, Math.Sin(angleA) * halfHorizontal).Multiply4(transform);
			vector bottomB = Vector(Math.Cos(angleB) * halfHorizontal, -halfHeight, Math.Sin(angleB) * halfHorizontal).Multiply4(transform);
			vector topA = Vector(Math.Cos(angleA) * halfHorizontal, halfHeight, Math.Sin(angleA) * halfHorizontal).Multiply4(transform);
			vector topB = Vector(Math.Cos(angleB) * halfHorizontal, halfHeight, Math.Sin(angleB) * halfHorizontal).Multiply4(transform);

			BPS_DrawDebugLine(bottomA, bottomB, color, flags);
			BPS_DrawDebugLine(topA, topB, color, flags);

			if ((i % 6) == 0)
				BPS_DrawDebugLine(bottomA, topA, color, flags);
		}
	}

	// =============================================================================================
	// CLEANUP
	// =============================================================================================
	void ~BPS_SafeZoneTriggerEntity()
	{
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
