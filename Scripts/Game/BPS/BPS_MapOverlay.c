//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
// Map Safe Zone boundary rendering for CYLINDER and SQUARE trigger shapes.
//------------------------------------------------------------------------------------------------
modded class SCR_MapEntity
{
	protected CanvasWidget m_BPSOverlayCanvas;

	protected ref array<ref CanvasWidgetCommand> m_aBPSDrawCommands =
		new array<ref CanvasWidgetCommand>();

	protected ref array<BPS_SafeZoneTriggerEntity> m_aBPSMapZones =
		new array<BPS_SafeZoneTriggerEntity>();

	protected ref array<vector> m_aBPSWorldOutline =
		new array<vector>();

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		if (!IsOpen())
		{
			BPS_DestroyOverlay();
			return;
		}

		if (!BPS_EnsureOverlay())
			return;

		BPS_UpdateOverlay();
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_EnsureOverlay()
	{
		if (m_BPSOverlayCanvas)
			return true;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		CanvasWidget mapWidget = GetMapWidget();
		Widget overlayParent = GetMapMenuRoot();
		int overlayZOrder = -1000;

		if (mapWidget)
		{
			Widget mapParent = mapWidget.GetParent();
			if (mapParent)
				overlayParent = mapParent;

			// Keep BPS one layer above the native map but below spawn buttons/text.
			overlayZOrder = mapWidget.GetZOrder() + 1;
		}

		if (!overlayParent)
			return false;

		Widget overlay = workspace.CreateWidget(
			WidgetType.CanvasWidgetTypeID,
			WidgetFlags.VISIBLE |
				WidgetFlags.IGNORE_CURSOR |
				WidgetFlags.NOFOCUS,
			null,
			overlayZOrder,
			overlayParent
		);

		m_BPSOverlayCanvas = CanvasWidget.Cast(overlay);
		if (!m_BPSOverlayCanvas)
			return false;

		m_BPSOverlayCanvas.SetName("BPS_SafeZoneMapOverlay");
		m_BPSOverlayCanvas.SetZOrder(overlayZOrder);

		FrameSlot.SetAnchorMin(m_BPSOverlayCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_BPSOverlayCanvas, 1, 1);
		FrameSlot.SetOffsets(m_BPSOverlayCanvas, 0, 0, 0, 0);

		m_BPSOverlayCanvas.SetFlags(
			WidgetFlags.IGNORE_CURSOR |
			WidgetFlags.NOFOCUS
		);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_UpdateOverlay()
	{
		if (!m_BPSOverlayCanvas)
			return;

		m_aBPSDrawCommands.Clear();
		BPS_SafeZoneTriggerEntity.BPS_CopyMapZones(m_aBPSMapZones);

		foreach (BPS_SafeZoneTriggerEntity zone : m_aBPSMapZones)
		{
			if (!zone || !zone.BPS_ShouldShowMapBoundary())
				continue;

			m_aBPSWorldOutline.Clear();
			zone.BPS_BuildMapWorldOutline(m_aBPSWorldOutline);

			if (m_aBPSWorldOutline.Count() < 3)
				continue;

			ref array<float> vertices = new array<float>();

			foreach (vector worldPoint : m_aBPSWorldOutline)
			{
				int screenX;
				int screenY;

				WorldToScreen(
					worldPoint[0],
					worldPoint[2],
					screenX,
					screenY,
					true
				);

				vertices.Insert(screenX);
				vertices.Insert(screenY);
			}

			if (vertices.Count() < 6)
				continue;

			ref PolygonDrawCommand fillCommand = new PolygonDrawCommand();
			fillCommand.m_Vertices = vertices;
			fillCommand.m_iColor = zone.BPS_GetMapBackgroundColor().PackToInt();
			m_aBPSDrawCommands.Insert(fillCommand);

			ref LineDrawCommand borderCommand = new LineDrawCommand();
			borderCommand.m_Vertices = vertices;
			borderCommand.m_iColor = zone.BPS_GetMapBorderColor().PackToInt();
			borderCommand.m_fWidth = zone.BPS_GetMapBorderSize();
			borderCommand.m_bShouldEnclose = true;
			m_aBPSDrawCommands.Insert(borderCommand);
		}

		m_BPSOverlayCanvas.SetDrawCommands(m_aBPSDrawCommands);
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_DestroyOverlay()
	{
		if (!m_BPSOverlayCanvas)
			return;

		m_aBPSDrawCommands.Clear();
		m_BPSOverlayCanvas.SetDrawCommands(m_aBPSDrawCommands);
		m_BPSOverlayCanvas.RemoveFromHierarchy();
		m_BPSOverlayCanvas = null;
	}
}
