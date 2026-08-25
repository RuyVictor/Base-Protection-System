modded class SCR_PlayerController
{
	void BPS_ShowMessage(string title, string message, float duration)
	{
		if (!Replication.IsRunning())
		{
			RpcDo_BPS_ShowMessage(title, message, duration);
			return;
		}

		Rpc(RpcDo_BPS_ShowMessage, title, message, duration);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_BPS_ShowMessage(string title, string message, float duration)
	{
		string localizedTitle = WidgetManager.Translate(title);
		string localizedMessage = WidgetManager.Translate(message);

		SCR_HintManagerComponent.ShowCustomHint(
			localizedMessage,
			localizedTitle,
			duration,
			true
		);
	}

	void BPS_ShowMessageParam1(string title, string message, float duration, int param1)
	{
		if (!Replication.IsRunning())
		{
			RpcDo_BPS_ShowMessageParam1(title, message, duration, param1);
			return;
		}

		Rpc(RpcDo_BPS_ShowMessageParam1, title, message, duration, param1);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_BPS_ShowMessageParam1(string title, string message, float duration, int param1)
	{
		string localizedTitle = WidgetManager.Translate(title, param1);
		string localizedMessage = WidgetManager.Translate(message, param1);

		SCR_HintManagerComponent.HideHint();
		SCR_HintManagerComponent.ShowCustomHint(
			localizedMessage,
			localizedTitle,
			duration,
			true
		);
	}
}
