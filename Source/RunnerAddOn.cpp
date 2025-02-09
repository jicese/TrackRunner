// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Chris Roberts

#include "RunnerAddOn.h"
#include "Constants.h"
#include "Preferences.h"

#include <Alert.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <Mime.h>
#include <NodeInfo.h>
#include <Path.h>
#include <PathFinder.h>
#include <Roster.h>
#include <TrackerAddOn.h>
#include <private/shared/CommandPipe.h>
#include "ProgressWindow.h"

#ifdef USE_MENUITEM_ICONS
#include <private/tracker/IconMenuItem.h>
#endif

enum {
	kCommandWhat = 'CMND',
	kUserGuideWhat = 'GUID',
	kManageCommandsWhat = 'MCMD',
	kGithubWhat = 'GTHB',
	kSuperMenuWhat = 'SUPR'
};


status_t
RunnerAddOn::RunCommand(BMessage* message)
{
	BMessage itemMessage;
	if (message->FindMessage(kCommandDataKey, &itemMessage) != B_OK)
		return B_ERROR;

	BString commandString;
	if (itemMessage.FindString(kEntryCommandKey, &commandString) != B_OK)
		return B_ERROR;

	// for now we assume that an empty command is intentional
	if (commandString.Length() == 0)
		return B_OK;

	BString nameString;
	if (itemMessage.FindString(kEntryNameKey, &nameString) != B_OK)
		return B_ERROR;

	entry_ref cwd_ref;
	if (message->FindRef("dir_ref", &cwd_ref) != B_OK)
		return B_ERROR;

	BPath cwdPath(&cwd_ref);
	if (cwdPath.InitCheck() != B_OK)
		return B_ERROR;

	if (itemMessage.GetBool(kEntryUseTerminalKey)) {
		// add selected files as command arguments
		entry_ref ref;
		for (int32 index = 0; message->FindRef("refs", index, &ref) == B_OK; index++) {
			BPath path(&ref);
			BString pathString(path.Path());
			// escape any single quotes
			pathString.ReplaceAll("'", "'\\''");
			commandString << " '" << pathString << "'";
		}
		//TODO make the read command optional but enabled by default
		commandString << "\n echo \"<<< Finished with status: $? >>>\"; read -p '<<<  Press ENTER to close!  >>>'";

		// give our Terminal a nice title
		BString titleString(nameString);
		titleString << " : " << cwdPath.Path();

		const char* argv[] = { "-w", cwdPath.Path(), "-t", titleString.String(), "/bin/sh", "-c", commandString, NULL };
		be_roster->Launch(kTerminalSignature, 7, argv);
	} 
	else {
		type_code type;
		int32 count = 0;
		
		// TODO allow no files selected
        message->GetInfo("refs", &type, &count);
		if(count == 0) {
			BAlert *alert = new BAlert("TrackRunner", "No file selected.", "OK");
			alert->Go();
			return B_OK;
		}		
		
		ProgressWindow* progressWindow = NULL;
		BString progressTitle("Executing ");
		progressTitle << nameString;
		progressTitle << " on ";
	    progressTitle << count << " files";
	    progressWindow = new ProgressWindow(&progressTitle);
	    progressWindow->Show();

		entry_ref ref;
		int32 i = 0;
    	for (int32 index = 0; message->FindRef("refs", index, &ref) == B_OK; index++) {
			BPath path(&ref);
//        	BString command("/boot/home/ShellIt.sh ");
        	BString command(commandString);
        	command << " " << path.Path() << "/" << ref.name;

        	system(command.String());
			i++;
			if(progressWindow->Lock())
			{
				BMessage *pmessage = new BMessage(kMsgProgressUpdate);
				pmessage->AddFloat("percent", ((float)i / (float)count)*100);
				progressWindow->PostMessage(pmessage);
				progressWindow->Unlock();
			}
		}
		progressWindow->Lock();
		progressWindow->Quit();
		
		/*
		BString cd;
		BString pathString(cwdPath.Path());
		// escape any single quotes
		pathString.ReplaceAll("'", "'\\''");
		cd.SetToFormat("cd '%s' && ", pathString.String());
		commandString.Prepend(cd);

		BPrivate::BCommandPipe pipe;
		pipe.AddArg("/bin/sh");
		pipe.AddArg("-c");
		pipe.AddArg(commandString);
		pipe.RunAsync();
		*/
	}

	return B_OK;
}


status_t
RunnerAddOn::OpenUserGuide(bool useAppImage)
{
	BPath indexLocation;

#if defined(PACKAGE_DOCUMENTATION_DIR)
	indexLocation.SetTo(PACKAGE_DOCUMENTATION_DIR);
#else
	image_info image;
	int32 cookie = 0;
	while (get_next_image_info(B_CURRENT_TEAM, &cookie, &image) == B_OK) {
		// locate our application image by type or by location in memory if we were loaded as an addon
		if ((useAppImage && image.type == B_APP_IMAGE) ||
			(!useAppImage && (char*)RunnerAddOn::OpenUserGuide >= (char*)image.text &&
			 					(char*)RunnerAddOn::OpenUserGuide <= (char*)image.text + image.text_size)) {
			BPath exePath(image.name);
			exePath.GetParent(&indexLocation);
			break;
		}
	}
#endif

	indexLocation.Append("UserGuide/index.html");

	// verify the index actually exists on disk
	BEntry entry(indexLocation.Path());
	if (entry.InitCheck() != B_OK || !entry.Exists()) {
		// search other document locations using the BPathFinder API if needed
		BStringList list;
		if (BPathFinder::FindPaths(B_FIND_PATH_DOCUMENTATION_DIRECTORY, "TrackRunner/UserGuide/index.html", B_FIND_PATH_EXISTING_ONLY, list) != B_OK) {
			(new BAlert("Error", "Unable to locate UserGuide html files", "Ok", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
			return B_ERROR;
		}
		indexLocation.SetTo(list.First());
	}

	const char* args[] = { indexLocation.Path(), NULL };

	status_t rc = be_roster->Launch("application/x-vnd.Be.URL.https", 1, args);
	if (rc != B_OK && rc != B_ALREADY_RUNNING) {
		(new BAlert("Error", "Failed to launch URL", "Ok", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
		return rc;
	}

	return B_OK;
}


void
populate_menu(BMessage* message, BMenu* menu, BHandler* handler)
{
	if (menu == NULL)
		return;

	// get rid of our old menu and start from scratch
	BMenuItem* menuItem = menu->FindItem(kSuperMenuWhat);
	if (menuItem != NULL)
		menu->RemoveItem(menuItem);

	BMessage prefsMessage;
	Preferences::ReadPreferences(prefsMessage);

	BMenu* trackMenu = new BMenu(prefsMessage.GetString(kMenuLabelKey, kAppTitle));
	BLayoutBuilder::Menu<> builder = BLayoutBuilder::Menu<>(trackMenu);

#ifdef USE_MENUITEM_ICONS
	bool useIcons = prefsMessage.GetBool(kIconMenusKey, kIconMenusDefault);
#endif

	if (prefsMessage.HasMessage(kEntryKey)) {
		BMessage itemMessage;
		for (int32 index = 0; prefsMessage.FindMessage(kEntryKey, index, &itemMessage) == B_OK; index++) {
			if (itemMessage.what != kCommandWhat)
				continue;

			BString commandString;
			if (itemMessage.FindString(kEntryCommandKey, &commandString) != B_OK)
				continue;

			// add each command with a copy of its configuration
			BMessage* menuMessage = new BMessage(*message);
			menuMessage->AddInt32(kAddOnWhatKey, kCommandWhat);
			menuMessage->AddMessage(kCommandDataKey, &itemMessage);

#ifdef USE_MENUITEM_ICONS
			if (useIcons) {
				IconMenuItem* item = NULL;
				BString commandFile(commandString);
				// remove escape characters from spaces
				BEntry entry(commandFile.ReplaceAll("\\ ", " "));
				int32 spacePosition = -1;
				int32 spaceOffset = 0;
				// attempt to find the actual file by splitting the command/arguments at the spaces
				while (!entry.Exists() && (spacePosition = commandString.FindFirst(' ', spaceOffset)) != B_ERROR) {
					commandFile = commandString;
					commandFile.Remove(spacePosition, commandFile.Length() - spacePosition);
					commandFile.ReplaceAll("\\ ", " ");
					spaceOffset = spacePosition + 1;
					entry.SetTo(commandFile);
				}

				BNode node(&entry);
				if (node.InitCheck() == B_OK) {
					BNodeInfo info(&node);
					if (info.InitCheck() == B_OK)
						item = new IconMenuItem(itemMessage.FindString(kEntryNameKey), menuMessage, &info, B_MINI_ICON);
				}

				// fall back to text/plain if we couldn't look up the proper type
				if (item == NULL)
					item = new IconMenuItem(itemMessage.FindString(kEntryNameKey), menuMessage, "text/plain");

				builder.AddItem(item);
			} else {
#endif
				// add a standard(no icon) menuitem
				builder.AddItem(itemMessage.FindString(kEntryNameKey), menuMessage);
#ifdef USE_MENUITEM_ICONS
			}
#endif
		}
	} else {
		builder.AddItem("<no commands>", 'MPTY').SetEnabled(false);
	}

	BMessage* prefsMenuMessage = new BMessage(*message);
	prefsMenuMessage->AddInt32(kAddOnWhatKey, kLaunchPrefsWhat);

	BMessage* guideMenuMessage = new BMessage(*message);
	guideMenuMessage->AddInt32(kAddOnWhatKey, kUserGuideWhat);

	BMessage* githubMenuMessage = new BMessage(*message);
	githubMenuMessage->AddInt32(kAddOnWhatKey, kGithubWhat);

	BMessage* commandsMenuMessage = new BMessage(*message);
	commandsMenuMessage->AddInt32(kAddOnWhatKey, kManageCommandsWhat);

	// clang-format off
	BMenu* prefsSubMenu = NULL;
	builder
		.AddSeparator()
		.AddMenu("Preferences & Help")
			.GetMenu(prefsSubMenu)
			.AddItem("Manage Commands" B_UTF8_ELLIPSIS, commandsMenuMessage)
			.AddItem("Preferences" B_UTF8_ELLIPSIS, prefsMenuMessage)
			.AddSeparator()
			.AddItem("User Guide", guideMenuMessage)
			.AddItem("Github Page", githubMenuMessage)
		.End()
	.End();
	// clang-format on

	prefsSubMenu->SetTargetForItems(handler);
	trackMenu->SetTargetForItems(handler);

#ifdef USE_MENUITEM_ICONS
	if (useIcons) {
		menu->AddItem(new IconMenuItem(trackMenu, new BMessage(kSuperMenuWhat), kAppSignature, B_MINI_ICON));
	} else {
#endif
		menu->AddItem(trackMenu);
		// set a message for our menu so we can find and delete it again the next time through
		menu->FindItem(trackMenu->Name())->SetMessage(new BMessage(kSuperMenuWhat));
#ifdef USE_MENUITEM_ICONS
	}
#endif
}


void
message_received(BMessage* message)
{
	int32 what;
	if (message->FindInt32(kAddOnWhatKey, &what) != B_OK)
		return;

	switch (what) {
		case kCommandWhat:
			//TODO improve alert message
			if (RunnerAddOn::RunCommand(message) != B_OK)
				(new BAlert("ErrorAlert", "Error running command", "OK", NULL, NULL, B_WIDTH_FROM_LABEL, B_STOP_ALERT))->Go();
			break;
		case kManageCommandsWhat:
		{
			BMessage launchMessage(kLaunchManageWhat);
			status_t rc = be_roster->Launch(kAppSignature, &launchMessage);
			if (rc != B_OK && rc != B_ALREADY_RUNNING)
				(new BAlert("ErrorAlert", "Unable to launch TrackRunner application", "OK", NULL, NULL, B_WIDTH_FROM_LABEL, B_STOP_ALERT))->Go();
		}
			break;
		case kLaunchPrefsWhat:
		{
			BMessage launchMessage(kLaunchPrefsWhat);
			status_t rc = be_roster->Launch(kAppSignature, &launchMessage);
			if (rc != B_OK && rc != B_ALREADY_RUNNING)
				(new BAlert("ErrorAlert", "Unable to launch TrackRunner application", "OK", NULL, NULL, B_WIDTH_FROM_LABEL, B_STOP_ALERT))->Go();
		}
			break;
		case kUserGuideWhat:
			RunnerAddOn::OpenUserGuide();
			break;
		case kGithubWhat:
		{
			const char* args[] = { kGithubUrl, NULL };
			status_t rc = be_roster->Launch("application/x-vnd.Be.URL.https", 1, args);
			if (rc != B_OK && rc != B_ALREADY_RUNNING)
				(new BAlert("Error", "Failed to launch URL", "Ok", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
		}
			break;
		default:
			break;
	}
}


void
process_refs(entry_ref directory, BMessage* refs, void* /*reserved*/)
{
	BMessage launchMessage(B_REFS_RECEIVED);

	if (refs != NULL)
		launchMessage = *refs;

	launchMessage.AddRef("TrackRunner:cwd", &directory);

	status_t rc = be_roster->Launch(kAppSignature, &launchMessage);
	if (rc != B_OK && rc != B_ALREADY_RUNNING)
		(new BAlert("ErrorAlert", "Unable to launch TrackRunner application", "OK", NULL, NULL, B_WIDTH_FROM_LABEL, B_STOP_ALERT))->Go();
}
