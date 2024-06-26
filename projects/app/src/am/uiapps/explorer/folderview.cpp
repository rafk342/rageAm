#include "folderview.h"

#include "am/asset/types/txd.h"
#include "am/asset/ui/assetwindowfactory.h"
#include "am/system/undo.h"
#include "am/ui/extensions.h"
#include "am/ui/font_icons/icons_am.h"
#include "helpers/format.h"
#include "am/uiapps/scene.h"

#ifdef AM_INTEGRATED
#include "am/integration/ui/modelscene.h"
#include "am/integration/ui/modelinspector.h"
#endif

const rageam::ui::EntrySelection& rageam::ui::FolderView::GetSelectedEntries() const
{
	return m_DragSelecting ? m_DragSelection : m_Selection.Entries;
}

void rageam::ui::FolderView::RenameEntry(const ExplorerEntryPtr& entry, ConstString newName)
{
	// We have to re-add entry in selection set because new name will affect hash set
	bool wasSelected = m_Selection.Entries.IsSelected(entry);
	if (wasSelected)
		m_Selection.Entries.SetSelected(entry, false);
	entry->Rename(newName);
	m_Selection.Entries.SetSelected(entry, true);

	m_SortIsDirty = true;
}

void rageam::ui::FolderView::CalculateSelectedSize()
{
	m_SelectionSize = 0;

	for (const ExplorerEntryPtr& entry : GetSelectedEntries())
	{
		m_SelectionSize += entry->GetSize();
	}
}

void rageam::ui::FolderView::SetSelectionStateWithUndo(const Selection& newState)
{
	if (m_Selection == newState)
		return;

	UndoStack* undo = UndoStack::GetCurrent();
	undo->Add(new UndoableState(m_Selection, newState, [this]
		{
			CalculateSelectedSize();
		}));
}

void rageam::ui::FolderView::UpdateEntrySelection(const ExplorerEntryPtr& entry)
{
	bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift);
	bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);

	// Toggling only requires CTRL to be pressed
	// Range (shift) selection requires at least ony entry to be selected
	bool canToggle = ctrl;
	bool canRange = shift && m_Selection.LastClickedID != -1;

	s32 currentId = entry->GetID();

	// Single-Selection
	if (!(canToggle || canRange))
	{
		Selection newState
		{
			currentId, { entry }
		};
		SetSelectionStateWithUndo(newState);
		return;
	}

	// Ctrl toggles selection of single entry
	if (ctrl)
	{
		Selection newState = m_Selection;
		newState.Entries.ToggleSelection(entry);
		newState.LastClickedID = currentId;

		SetSelectionStateWithUndo(newState);
		return;
	}

	Selection newState = m_Selection;

	// Range selection, we select all entries from first selected entry to current one
	s32 startIndex = m_RootEntry->GetIndexFromID(newState.LastClickedID);
	s32 endIndex = m_RootEntry->GetIndexFromID(currentId);

	startIndex = m_RootEntry->TransformToSorted(startIndex);
	endIndex = m_RootEntry->TransformToSorted(endIndex);

	// Range selection overrides any existing selection
	newState.Entries = { m_RootEntry->GetChildFromID(newState.LastClickedID) };

	// Note here that we subtract / increment index because we already added it above
	if (endIndex < startIndex)
	{
		std::swap(startIndex, endIndex);
		endIndex--; // startIndex is now endIndex
	}
	else
	{
		startIndex++;
	}

	// Loop through sorted region and add all those entries to selected
	for (s32 i = startIndex; i <= endIndex; i++)
	{
		u16 actualIndex = m_RootEntry->TransformFromSorted(i);
		newState.Entries.SetSelected(m_RootEntry->GetChildFromIndex(actualIndex), true);
	}

	SetSelectionStateWithUndo(newState);
}

void rageam::ui::FolderView::UpdateEntryRenaming(const ExplorerEntryPtr& entry, const SlRenamingSelectableState& state)
{
	// We've began renaming
	if (state.Renaming && !state.WasRenaming)
	{
		m_RenamingEntry = entry;
		return;
	}

	if (!state.StoppedRenaming())
		return;

	// Set only if name actually changed
	if (state.AcceptRenaming && !String::Equals(state.TextEditable, state.Buffer))
	{
		ExplorerEntryPtr renameEntry = entry;
		rage::atString oldName = state.TextEditable;
		rage::atString newName = state.Buffer;
		UndoStack::GetCurrent()->Add(new UndoableFn(
			[=, this]
			{
				RenameEntry(entry, newName);
			},
			[=, this]
			{
				RenameEntry(entry, oldName);
			}));
	}
	m_RenamingEntry.reset();
}

void rageam::ui::FolderView::CreateEditableState(const ExplorerEntryPtr& entry, SlRenamingSelectableState& state)
{
	// There's no point to allow user to edit file extension so we display it with extension
	// and in rename mode there's no extension (state::TextDisplay & state::TextEditable)

	const EntrySelection& selection = GetSelectedEntries();

	state.Icon = entry->GetIcon().GetID();
	state.IconUV2 = entry->GetIcon().GetUV2();
	state.Buffer = m_RenameBuffer;
	state.BufferSize = IM_ARRAYSIZE(m_RenameBuffer);
	state.Selected = selection.IsSelected(entry);
	state.Renaming = m_RenamingEntry == entry;
	state.TextDisplay = entry->GetFullName();
	state.TextEditable = m_AllowRenaming ? entry->GetName() : entry->GetFullName();
	state.SpanAllColumns = true;
}

void rageam::ui::FolderView::RenderEntryTableRow(const ExplorerEntryPtr& entry)
{
	SlRenamingSelectableState state;
	CreateEditableState(entry, state);

	SlGuiRenamingSelectableFlags selectableFlags = SlGuiRenamingSelectableFlags_RightClickSelect;
	// We want to disable other items if we rename one
	if (state.Renaming && !state.Selected)
		selectableFlags |= SlGuiRenamingSelectableFlags_Disabled;

	if (!m_AllowRenaming || entry->GetFlags() & ExplorerEntryFlags_NoRename)
		selectableFlags |= SlGuiRenamingSelectableFlags_NoRename;

	// Selection was reset and now we just draw outline around last selected item
	if (!state.Selected && m_Selection.LastClickedID == entry->GetID())
		selectableFlags |= SlGuiRenamingSelectableFlags_Outline;

	// Disable other entries while renaming
	bool disabled = !state.Renaming && m_RenamingEntry != nullptr;

	if (disabled) ImGui::BeginDisabled();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, 0); // Empty default entry background
	if (SlGui::RenamingSelectable(state, selectableFlags))
	{
		UpdateEntrySelection(entry);
	}
	ImGui::PopStyleColor();
	if (disabled) ImGui::EndDisabled();

	if (state.DoubleClicked)
	{
		m_DoubleClickedEntry = entry;
	}
	UpdateEntryRenaming(entry, state);
}

void rageam::ui::FolderView::RenderStatusBar() const
{
	ImGui::Indent();
	u16 childCount = m_RootEntry->GetChildCount();
	u16 selectedCount = GetSelectedEntries().GetCount();
	if (selectedCount != 0)
	{
		if (m_SelectionSize == 0) // 5 of 10 selected
		{
			ImGui::Text("%u of %u selected", selectedCount, childCount);
		}
		else // 5 of 10 selected  |  2.15MB
		{
			ImGui::Text("%u of %u selected ", selectedCount, childCount);

			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();

			char sizeBuffer[16];
			FormatSize(sizeBuffer, 16, m_SelectionSize);
			ImGui::Text(" %s", sizeBuffer);
		}
	}
	else // 5 items
	{
		ImGui::Text("%u %s ", childCount, childCount == 1 ? "item" : "items");
	}
	ImGui::Unindent();
}

void rageam::ui::FolderView::UpdateSelectAll()
{
	if (ImGui::Shortcut(ImGuiKey_A | ImGuiMod_Ctrl) && ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
	{
		Selection newState = { m_Selection.LastClickedID };

		for (const ExplorerEntryPtr& entry : *m_RootEntry)
			newState.Entries.SetSelected(entry, true);

		SetSelectionStateWithUndo(newState);
	}
}

void rageam::ui::FolderView::UpdateQuickView()
{
	bool canOpenQuickView =
		m_Selection.LastClickedID != -1 &&
		m_Selection.Entries.Any();
	if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && canOpenQuickView)
	{
		if (!m_QuickLook.IsOpened())
			m_QuickLook.Open(m_RootEntry->GetChildFromID(m_Selection.LastClickedID));
		else
			m_QuickLook.Close();
	}
	m_QuickLook.Render();
}

void rageam::ui::FolderView::UpdateEntryOpening()
{
	if (!m_DoubleClickedEntry)
		return;

	file::WPath entryPath = file::PathConverter::Utf8ToWide(m_DoubleClickedEntry->GetPath());

	// Try to open IDR / YDR viewers
	SceneType sceneType = Scene::GetSceneType(entryPath);
	if (sceneType != Scene_Invalid)
	{
		Scene::ConstructFor(entryPath);
		m_DoubleClickedEntry = nullptr;
		return;
	}

	if (m_DoubleClickedEntry->IsAsset())
	{
		AssetWindowFactory::OpenNewOrFocusExisting(m_DoubleClickedEntry->GetAsset());
		return;
	}

	// Last handler, don't reset m_DoubleClickedEntry here because TreeView will need it
	if (m_DoubleClickedEntry && m_DoubleClickedEntry->IsDirectory())
	{
		SetRootEntry(m_DoubleClickedEntry);
	}
}

void rageam::ui::FolderView::BeginDragSelection(ImRect& dragSelectRect)
{
	ImRect& clipRect = m_TableRect;

	// Mouse must hover table region but not table entries
	bool canStartDragSelection =
		ImGui::IsMouseHoveringRect(clipRect.Min, clipRect.Max, false) &&
		!ImGui::IsMouseHoveringRect(m_TableContentRect.Min, m_TableContentRect.Max, false) &&
		ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) && 
		!ImGui::IsAnyPopUpOpen(); // Fix selection working when header context menu is open

	// GImGui->CurrentWindow->DrawList->AddRect(clipRect.Min, clipRect.Max, 0xFF00FF00);

	ImGuiDragSelectionFlags dragSelectionFlags = 0;
	if (!canStartDragSelection)
		dragSelectionFlags |= ImGuiDragSelectionFlags_DisableBegin;
	bool dragStopped;
	bool wasDragSelecting = m_DragSelecting;
	m_DragSelecting = ImGui::DragSelectionBehaviour(
		ImGui::GetID("FOLDER_VIEW_DRAG_SELECTION"), dragStopped, dragSelectRect, clipRect, dragSelectionFlags);

	bool startedDragSelecting = !wasDragSelecting && m_DragSelecting;
	if (startedDragSelecting)
	{
		m_DragSelection.AllocateForDirectory(m_RootEntry);
	}

	// Finish drag selection, we do that only on drag finishing because otherwise
	// we'll spam whole undo stack in less than a second.
	// To preview currently drag-selected entries we override ::IsCurrentEntrySelected behaviour while dragging
	if (dragStopped)
	{
		Selection newState
		{
			m_Selection.LastClickedID, std::move(m_DragSelection)
		};
		SetSelectionStateWithUndo(newState);
	}
}

void rageam::ui::FolderView::EndDragSelection(bool selectionChangedDuringDragging)
{
	if (m_DragSelecting && selectionChangedDuringDragging)
	{
		CalculateSelectedSize();
	}
}

void rageam::ui::FolderView::RenderEntries()
{
	if (!m_RootEntry)
		return;

	m_DoubleClickedEntry = nullptr;

	// Setup drag selection
	ImRect dragSelectRect;
	bool selectionChangedDuringDragging = false;
	BeginDragSelection(dragSelectRect);

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Sortable |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_NoBordersInBody |
		ImGuiTableFlags_Hideable |
		ImGuiTableFlags_Reorderable | 
		ImGuiTableFlags_ScrollY; // For TableSetupScrollFreeze


	// Leave a bit of empty space left to the entries
	float tableLeftMarginSize = GImGui->FontSize * 0.75f;
	ImGui::Indent(tableLeftMarginSize);

	m_TableContentRect = {};
	if (ImGui::BeginTable("FolderTable", 4, tableFlags))
	{
		// We leave a bit of empty space on the right to allow user to use drag selection
		float tableRightMarginSize = GImGui->FontSize * 2.0f;
		GImGui->CurrentTable->WorkRect.Max.x -= tableRightMarginSize;
		GImGui->CurrentTable->InnerClipRect.Max.x -= tableRightMarginSize;

		// Reset scroll, otherwise if if we're on bottom of some huge folder and switch
		// to smaller one, scroll gonna break
		if (m_RootEntryChangedThisFrame)
		{
			ImGui::SetScrollY(0);
		}

		// Force-sort entry
		if (m_RootEntryChangedThisFrame || m_SortIsDirty)
		{
			ImGuiTable* table = ImGui::GetCurrentTable();
			table->IsSortSpecsDirty = true;
			m_SortIsDirty = false;
		}

		ImGuiTableColumnFlags nameFlags =
			ImGuiTableColumnFlags_DefaultSort |
			ImGuiTableColumnFlags_PreferSortAscending | 
			ImGuiTableColumnFlags_NoHide;

		ImGui::TableSetupColumn("Name", nameFlags, 0, ExplorerEntryColumnID_Name);
		ImGui::TableSetupColumn("Date Modified", 0, 0, ExplorerEntryColumnID_DateModified);
		ImGui::TableSetupColumn("Type", 0, 0, ExplorerEntryColumnID_TypeName);
		ImGui::TableSetupColumn("Size", 0, 0, ExplorerEntryColumnID_Size);
		ImGui::TableSetupScrollFreeze(0, 1);

		if (ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs())
		{
			m_RootEntry->Sort(sort);
		}

		// TODO: SlGui header is disabled for now, this one looks awful
		ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, 0); // Remove default table header gray background
		ImGui::TableHeadersRow();
		ImGui::PopStyleColor();
		m_HoveringTableHeaders = ImGui::TableIsHoveringRow();
		// Header must be added separately
		m_TableContentRect.Add(ImGui::TableGetRowRect());

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0)); // Remove 3km padding between entries
		for (u16 i = 0; i < m_RootEntry->GetChildCount(); i++)
		{
			ImGui::TableNextRow();

			ExplorerEntryPtr& entry = m_RootEntry->GetSortedChildFromIndex(i);

			// Column: Name & Selector
			ImGui::TableSetColumnIndex(0);
			{
				RenderEntryTableRow(entry);

				// Here we load some CPU/GPU expensive data (like icons)
				// There's no point to load it up for entries that aren't even on screen
				if (ImGui::IsItemVisible())
					entry->PrepareToBeDisplayed();

				// Entry is selectable and it covers whole table width
				ImRect entryRect = GImGui->LastItemData.Rect;

				// Handle mouse-drag selection
				if (m_DragSelecting)
				{
					bool nowSelected = dragSelectRect.Overlaps(entryRect);
					bool wasSelected = m_DragSelection.IsSelected(entry);
					if (nowSelected != wasSelected)
					{
						m_DragSelection.SetSelected(entry, nowSelected);
						selectionChangedDuringDragging = true;
					}
				}

				// Allow entry(s) to be drag-dropped
				ExplorerEntryDragData* dragData;
				if (ExplorerEntryBeginDragSource(entry, &dragData))
				{
					if (dragData)
					{
						// NOTE: Multi-selection dragging only works when we're dragging one of selected entries!
						// otherwise selection is set to single dragged entry
						if (!m_Selection.Entries.IsSelected(entry))
						{
							m_Selection.LastClickedID = entry->GetID();
							m_Selection.Entries.ClearSelection();
							m_Selection.Entries.SetSelected(entry, true);
						} // Else we do multi-selection drag

						dragData->Entries.Reserve(m_Selection.Entries.GetCount());
						for (const ExplorerEntryPtr& dragEntry : m_Selection.Entries)
							dragData->Entries.Add(dragEntry);
						dragData->Cut = false; // TODO: This must be different for QuickAccess (User) / Fi
					}

					ExplorerEntryEndDragSource();
				}
			}

			// We display remaining columns semi-transparent
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.75f);

			// Column: Date Modified
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", entry->GetTimeDisplay());

			// Column: Type
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s", entry->GetTypeName());

			// Column: Size
			ImGui::TableSetColumnIndex(3);
			if (!entry->IsDirectory())
			{
				char sizeBuffer[16];
				FormatSize(sizeBuffer, 16, entry->GetSize());
				ImGui::Text("%s", sizeBuffer);
			}

			// Right padding, just for the look
			ImGui::SameLine(); ImGui::Dummy(ImVec2(10, 0));

			ImGui::PopStyleVar(); // Alpha

			// Add entry row to content rect
			m_TableContentRect.Add(ImGui::TableGetRowRect());
		}
		ImGui::PopStyleVar();

		ImGuiTable* table = GImGui->CurrentTable;
		m_TableRect = table->InnerRect;
		// Make sure that content rect doesn't contain rows outside work area
		m_TableContentRect.ClipWith(table->InnerClipRect);
		// We use table rect for selection and because we added small padding on the left above
		// we have to add it back here, otherwise this area will be unselectable
		m_TableRect.Min.x -= tableLeftMarginSize;

		ImGui::EndTable(); // FolderTable

		// GImGui->CurrentWindow->DrawList->AddRect(m_TableContentRect.Min, m_TableContentRect.Max, 0xFF0000FF);
		// GImGui->CurrentWindow->DrawList->AddRect(GImGui->CurrentWindow->WorkRect.Min, GImGui->CurrentWindow->WorkRect.Max, 0xFFFFFF00);

		if (m_RootEntry->GetChildCount() == 0)
		{
			ImGui::Dummy(ImVec2(0, 15));
			ImGui::TextCentered("This folder is empty.", ImGuiTextCenteredFlags_Horizontal);
		}
	}
	ImGui::Unindent();

	EndDragSelection(selectionChangedDuringDragging);
	UpdateSelectAll();
	UpdateQuickView();
}

void rageam::ui::FolderView::UpdateSearchOnType()
{
	// TODO: ...
}

void rageam::ui::FolderView::Render()
{
	m_DoubleClickedEntry = nullptr;

	UpdateSearchOnType();

	// Refresh directory F5 was pressed or some file was changed
	bool changeOccured = m_DirectoryWatcher.GetChangeOccuredAndReset();
	bool f5Pressed = ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F5, false);
	if (changeOccured || f5Pressed)
	{
		Refresh();
	}

	// Refresh formatted time on entries if necessary
	double time = ImGui::GetTime();
	if (time > m_NextEntryTimeUpdate)
	{
		m_NextEntryTimeUpdate = time + ENTRY_TIME_REFRESH_INTERVAL;

		for (const ExplorerEntryPtr& entry : *m_RootEntry)
		{
			entry->RefreshDisplayInfo();
		}
	}

	// Draw context menu
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
	ImGui::SetNextWindowSize(ImVec2(190, 0));
	if (ImGui::BeginPopup(CONTEXT_MENU_ID))
	{
		ExplorerEntryPtr selectedEntry = nullptr;
		if (m_Selection.LastClickedID != -1 && m_Selection.Entries.Any())
			selectedEntry = m_RootEntry->GetChildFromID(m_Selection.LastClickedID);

		if (selectedEntry)
		{
			ImGui::MenuItem(ICON_AM_OPEN"  Open");

			ImGui::Separator();

			ImGui::MenuItem(ICON_AM_CANCEL"  Remove");
			if (ImGui::MenuItem(ICON_AM_RENAME"  Rename", "F2"))
				m_RenamingEntry = selectedEntry;
		}
		else
		{
			if (ImGui::MenuItem(ICON_AM_REFRESH"  Refresh"))
				Refresh();
		}

		// TODO: Compile option
		/*if (selectedEntry && selectedEntry->IsAsset())
		{
			ImGui::Separator();

			asset::AssetPtr& project = selectedEntry->GetAsset();

			bool isCompiling = project->IsCompiling();

			if (isCompiling) ImGui::BeginDisabled();
			bool compile = ImGui::MenuItem("Compile");
			if (isCompiling)ImGui::EndDisabled();

			if (compile)
			{
				rage::atString compilePath = rage::Path::Combine(
					{ m_ContextMenuEntry->GetParent()->GetPath(), m_ContextMenuEntry->GetName() });
				compilePath += project->GetCompiledExtension();
				project->CompileToAsync(compilePath, [this]
					{
						m_NeedRefresh = true;
					});

				ImGui::CloseCurrentPopup();
			}
		}*/

		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2); // WindowPadding, ItemSpacing

	// Render folder view
	RenderEntries();

	// Open context menu
	if (!m_HoveringTableHeaders && 
		ImGui::IsMouseReleased(ImGuiMouseButton_Right) && 
		ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) &&
		!ImGui::IsAnyPopUpOpen())
	{
		if (m_RenamingEntry)
		{
			// Commit renaming
			SlRenamingSelectableState state;
			CreateEditableState(m_RenamingEntry, state);
			state.Renaming = false;
			state.WasRenaming = true;
			state.AcceptRenaming = true;
			UpdateEntryRenaming(m_RenamingEntry, state);
		}

		ImGui::OpenPopup(CONTEXT_MENU_ID);
	}

	m_RootEntryChangedThisFrame = false;
	// We proceed this after resetting entry changed flag because we may open directory here
	UpdateEntryOpening();
}

void rageam::ui::FolderView::SetRootEntry(const ExplorerEntryPtr& root)
{
	// TODO: Unload loaded assets for previous root entry

	m_RootEntryChangedThisFrame = true;

	m_RootEntry = root;
	m_RootEntry->LoadChildren();

	// Directory watcher only works with file system,
	// there's no point for watcher in user managed directory in either case
	if(m_RootEntry->GetEntryType() == ExplorerEntryType_Fi)
		m_DirectoryWatcher.SetEntry(m_RootEntry->GetPath());

	m_Selection.LastClickedID = -1;
	m_Selection.Entries.ClearSelection();
	m_Selection.Entries.AllocateForDirectory(m_RootEntry);

	// TODO: We either clean up selection undo or add navigation into undo too
	UndoStack* undo = UndoStack::GetCurrent();
	if (undo)
		undo->Clear();
}

void rageam::ui::FolderView::Refresh()
{
	m_RootEntry->UnloadChildren();
	m_RootEntry->LoadChildren();
	// This will trigger sorting update because RefreshChildren currently does hard-reset
	m_RootEntryChangedThisFrame = true;
}
