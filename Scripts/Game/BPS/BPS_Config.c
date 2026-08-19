//------------------------------------------------------------------------------------------------
// BPS - CONFIGURATION
//------------------------------------------------------------------------------------------------


// ================================================================================================
// FRIENDLY MESSAGES
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_FriendlyMessagesConfig
{
	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show messages when friendly players enter or leave."
	)]
	bool m_bEnabled;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Message duration."
	)]
	float m_fDuration;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sEnterTitle;


	[Attribute(
		"You entered a safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sEnterMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitTitle;


	[Attribute(
		"You left the safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitMessage;


	bool IsEnabled()
	{
		return m_bEnabled;
	}


	float GetDuration()
	{
		return m_fDuration;
	}


	string GetEnterTitle()
	{
		return m_sEnterTitle;
	}


	string GetEnterMessage()
	{
		return m_sEnterMessage;
	}


	string GetExitTitle()
	{
		return m_sExitTitle;
	}


	string GetExitMessage()
	{
		return m_sExitMessage;
	}
}


// ================================================================================================
// ENEMY INTRUSION
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_IntruderConfig
{
	[Attribute(
		"10",
		UIWidgets.EditBox,
		"Seconds an enemy can remain inside before being killed."
	)]
	int m_iKillDelaySeconds;


	[Attribute(
		"1.1",
		UIWidgets.EditBox,
		"Countdown message duration."
	)]
	float m_fCountdownMessageDuration;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Exit message duration."
	)]
	float m_fExitMessageDuration;


	[Attribute(
		"WARNING - ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sWarningTitle;


	[Attribute(
		"You are inside an enemy Safe Zone. Leave immediately. You will be killed in %1 seconds.",
		UIWidgets.EditBox,
		"Supports localization keys. %1 = remaining seconds."
	)]
	string m_sWarningMessage;


	[Attribute(
		"ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitTitle;


	[Attribute(
		"You left the enemy Safe Zone. Elimination cancelled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitMessage;


	int GetKillDelaySeconds()
	{
		if (m_iKillDelaySeconds < 1)
			return 1;

		return m_iKillDelaySeconds;
	}


	float GetCountdownMessageDuration()
	{
		return m_fCountdownMessageDuration;
	}


	float GetExitMessageDuration()
	{
		return m_fExitMessageDuration;
	}


	string GetWarningTitle()
	{
		return m_sWarningTitle;
	}


	string GetWarningMessage()
	{
		return m_sWarningMessage;
	}


	string GetExitTitle()
	{
		return m_sExitTitle;
	}


	string GetExitMessage()
	{
		return m_sExitMessage;
	}
}


// ================================================================================================
// COMBAT LOCK
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_CombatConfig
{
	[Attribute(
		"15",
		UIWidgets.EditBox,
		"Combat Lock duration. 0 disables Combat Lock."
	)]
	float m_fDuration;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Combat messages duration."
	)]
	float m_fMessageDuration;


	[Attribute(
		"SAFE ZONE - COMBAT",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sLockTitle;


	[Attribute(
		"You fired inside the Safe Zone. Your protection has been temporarily disabled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sLockMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sRestoredTitle;


	[Attribute(
		"Your Safe Zone protection has been restored.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sRestoredMessage;


	float GetDuration()
	{
		if (m_fDuration < 0)
			return 0;

		return m_fDuration;
	}


	float GetMessageDuration()
	{
		return m_fMessageDuration;
	}


	string GetLockTitle()
	{
		return m_sLockTitle;
	}


	string GetLockMessage()
	{
		return m_sLockMessage;
	}


	string GetRestoredTitle()
	{
		return m_sRestoredTitle;
	}


	string GetRestoredMessage()
	{
		return m_sRestoredMessage;
	}
}


// ================================================================================================
// FRIENDLY FIRE
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_FriendlyFireConfig
{
	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Block friendly damage against a friendly player who is inside the Safe Zone."
	)]
	bool m_bEnabled;


	[Attribute(
		"2",
		UIWidgets.EditBox,
		"Warning cooldown."
	)]
	float m_fWarningCooldown;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Warning duration."
	)]
	float m_fMessageDuration;


	[Attribute(
		"FRIENDLY FIRE BLOCKED",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sTitle;


	[Attribute(
		"You cannot damage allies inside the Safe Zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sMessage;


	bool IsEnabled()
	{
		return m_bEnabled;
	}


	float GetWarningCooldown()
	{
		return m_fWarningCooldown;
	}


	float GetMessageDuration()
	{
		return m_fMessageDuration;
	}


	string GetTitle()
	{
		return m_sTitle;
	}


	string GetMessage()
	{
		return m_sMessage;
	}
}


// ================================================================================================
// MAP DISPLAY
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_MapDisplayConfig
{
	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show the Safe Zone boundary on the map."
	)]
	bool m_bEnabled;


	[Attribute(
		"2",
		UIWidgets.Slider,
		"Circle border thickness in pixels.",
		"1 20 0.5"
	)]
	float m_fBorderSize;


	// =============================================================================================
	// FRIENDLY / OWNER COLORS
	//
	// ColorPicker selects RGB. Alpha is configured separately so transparency is
	// always editable even when the Workbench color picker does not expose alpha.
	// =============================================================================================

	[Attribute(
		"0 0.45 1 1",
		UIWidgets.ColorPicker,
		"Border RGB color when the Safe Zone belongs to the local player's faction."
	)]
	ref Color m_FriendlyBorderColor;


	[Attribute(
		"0.9",
		UIWidgets.Slider,
		"Friendly border alpha. 0 = invisible, 1 = opaque.",
		"0 1 0.01"
	)]
	float m_fFriendlyBorderAlpha;


	[Attribute(
		"0 0.45 1 1",
		UIWidgets.ColorPicker,
		"Background RGB color when the Safe Zone belongs to the local player's faction."
	)]
	ref Color m_FriendlyBackgroundColor;


	[Attribute(
		"0.15",
		UIWidgets.Slider,
		"Friendly background alpha. 0 = invisible, 1 = opaque.",
		"0 1 0.01"
	)]
	float m_fFriendlyBackgroundAlpha;


	// =============================================================================================
	// ENEMY COLORS
	// =============================================================================================

	[Attribute(
		"1 0 0 1",
		UIWidgets.ColorPicker,
		"Border RGB color when the Safe Zone belongs to an enemy faction."
	)]
	ref Color m_EnemyBorderColor;


	[Attribute(
		"0.9",
		UIWidgets.Slider,
		"Enemy border alpha. 0 = invisible, 1 = opaque.",
		"0 1 0.01"
	)]
	float m_fEnemyBorderAlpha;


	[Attribute(
		"1 0 0 1",
		UIWidgets.ColorPicker,
		"Background RGB color when the Safe Zone belongs to an enemy faction."
	)]
	ref Color m_EnemyBackgroundColor;


	[Attribute(
		"0.15",
		UIWidgets.Slider,
		"Enemy background alpha. 0 = invisible, 1 = opaque.",
		"0 1 0.01"
	)]
	float m_fEnemyBackgroundAlpha;


	bool IsEnabled()
	{
		return m_bEnabled;
	}


	float GetBorderSize()
	{
		if (m_fBorderSize < 1)
			return 1;

		return m_fBorderSize;
	}


	//------------------------------------------------------------------------------------------------
	protected float ClampAlpha(float alpha)
	{
		if (alpha < 0)
			return 0;

		if (alpha > 1)
			return 1;

		return alpha;
	}


	//------------------------------------------------------------------------------------------------
	protected Color WithAlpha(Color source, float alpha)
	{
		ref Color result;

		if (source)
			result = source.Copy();
		else
			result = new Color(1, 1, 1, 1);

		result.SetA(
			ClampAlpha(alpha)
		);

		return result;
	}


	//------------------------------------------------------------------------------------------------
	Color GetFriendlyBorderColor()
	{
		Color source = m_FriendlyBorderColor;

		if (!source)
			source = Color.FromRGBA(0, 115, 255, 255);

		return WithAlpha(
			source,
			m_fFriendlyBorderAlpha
		);
	}


	//------------------------------------------------------------------------------------------------
	Color GetFriendlyBackgroundColor()
	{
		Color source = m_FriendlyBackgroundColor;

		if (!source)
			source = Color.FromRGBA(0, 115, 255, 255);

		return WithAlpha(
			source,
			m_fFriendlyBackgroundAlpha
		);
	}


	//------------------------------------------------------------------------------------------------
	Color GetEnemyBorderColor()
	{
		Color source = m_EnemyBorderColor;

		if (!source)
			source = Color.FromRGBA(255, 0, 0, 255);

		return WithAlpha(
			source,
			m_fEnemyBorderAlpha
		);
	}


	//------------------------------------------------------------------------------------------------
	Color GetEnemyBackgroundColor()
	{
		Color source = m_EnemyBackgroundColor;

		if (!source)
			source = Color.FromRGBA(255, 0, 0, 255);

		return WithAlpha(
			source,
			m_fEnemyBackgroundAlpha
		);
	}
}
