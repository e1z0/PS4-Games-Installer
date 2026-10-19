# Work tracker

## Beginner setup video

- [x] Verify project setup instructions and current public release.
- [x] Record a separate real Compose setup and browser library verification.
- [x] Write beginner narration, source labels and matching consumer commands.
- [x] Finish narration/rendering, inspect captions/screens and validate the movie.
- [x] Export the video, chapters, captions, commands and capture credits.
- [x] Add a README video preview with companion notes immediately underneath.
- [x] Publish and verify the tutorial links on GitHub.
- [ ] Replace illustrated GoldHEN/USB steps with newly filmed PS4 footage if a
  capture/control connection becomes available.

## Readable package URL in queue diagnostics

- [x] Reuse the display decoder while preserving the original encoded URL.
- [x] Finish host tests and desktop capture review for the validated r48 package.
- [x] Deploy r48 through the configured private FTP target.
- [ ] Confirm the readable package path in the PS4 diagnostics dialog.

## Large downloads and one installation at a time

- [x] Retrieve real PS4 failure records and inspect ARK's server/header behavior.
- [x] Keep/recheck partial packages and retry transient transfer interruptions.
- [x] Separate downloads from a persistent, serial PS4 installation queue.
- [x] Complete host/native regressions, package validation and queue visual review.
- [x] Deploy the successful package using the private .env target.
- [ ] Confirm ARK completes and multiple downloaded games install sequentially on PS4.

## Internet Archive authenticated downloads

- [x] Confirm the same file downloads when the user signs into Archive.org.
- [x] Reuse the verified HTTP/staging path with a private, host-scoped session.
- [x] Add masked account sign-in/sign-out without changing the public catalog.
- [x] Validate authentication, redirects, storage, UI and r46 package build.
- [ ] Verify account sign-in and complete installation on the user's PS4 9.00.

## GoDaddy G2 chain compatibility

- [x] Retrieve r44 logs showing the initial host/unknown CA with successful CA loading.
- [x] Load only the pinned CA-published G2 intermediate alongside existing roots.
- [x] Validate certificate pins/signature, native prewarming and r45 packaging.
- [x] Confirm package HTTPS on firmware 9.00: r45 reaches HTTP responses; file hosts return 401/500.

## PS4 9.00 HTTPS follow-up

- [x] Record the r42 console failure and distinguish artwork from package TLS.
- [x] Preserve custom trust through the documented keepalive connection mode.
- [x] Correct the misleading failure dialog and add host/redirect/TLS diagnostics.
- [x] Validate focused native/host regressions, error-screen capture and r44 package.
- [x] Retest on firmware 9.00: r44 still failed trust; the r45 compatibility anchor passes TLS.

## HTTPS compatibility

- [x] Reproduce redirect and trust failures with local package fixtures.
- [x] Package pinned CA roots and load them before native HTTPS requests.
- [x] Follow bounded redirects and keep TLS verification enabled for packages/catalogs.
- [x] Reuse staging/local installation for HTTPS; retain direct HTTP BGFT handling.
- [x] Complete host/native regressions and compile/validate the r42 PS4 package.
- [ ] Verify HTTPS catalogs and redirected HTTPS installation on PS4 11.00 + GoldHEN.

## r40 release preparation

- [x] Write concise temporary realease_notes.md for the GitHub release page.
- [x] Ignore the notes in Git and retain the existing r40 package/revision.
- [x] User published r40 and removed the temporary release notes; verify absence.


## Current implementation: screenshot shortcut in game Actions

- [x] Add Show screenshots after Details when the game has supplied images.
- [x] Open the existing gallery at the first image and return to Actions with O.
- [x] Run UI/metadata regressions, build/validate and inspect action-menu captures.
- [x] Commit the successful revision and updated documentation locally.
- [ ] Confirm the direct shortcut and return behavior on PS4 11.00 + GoldHEN.


## Current implementation: quicker selected-title scrolling

- [x] Shorten the marquee pause to 0.35 seconds for selected titles/categories.
- [x] Verify motion before one second, build/validate and inspect captures.
- [x] Commit the successful revision and progress locally.
- [ ] Confirm the shorter perceived delay on PS4 11.00 + GoldHEN.


## Current implementation: screenshot download recovery

- [x] Inspect PS4 r37 logs, catalog URLs and screenshot disk-cache evidence.
- [x] Add visible failures/retry and bounded artwork diagnostics.
- [x] Separate the IGDB artwork HTTPS compatibility path from catalogs/packages.
- [x] Run relevant regressions, native build/validation and visual review.
- [x] Commit the successful revision and updated manuals locally.
- [ ] Retest IGDB screenshot display/reload/navigation on PS4 11.00 + GoldHEN.


## Current implementation: label removal and refresh/startup recovery

- [x] Pull the PS4 app/system logs and cached state without modifying console data.
- [x] Reproduce or narrow the refresh/startup failure and add recovery/regressions.
- [x] Remove local label editing/storage/rendering while accepting older records.
- [x] Use the full category row and scroll overflowing game/category text in tiles.
- [x] Separate Genres and assigned Tags; show modes, players and split screen.
- [x] Complete host/native checks, screenshot review, docs and a local revision commit.
- [ ] Retest startup, cached/remote refresh, tile text and revised Details on PS4.


## Current implementation: rich game details and genre browsing

- [x] Inspect actual embedded IGDB schema and package_type fields.
- [x] Implement ordered bold-key details, local overrides and collapsed sections.
- [x] Add media screens, IGDB category/theme/rating fallbacks and genre sidebar.
- [x] Route explicit update/DLC packages to Updates and persist queue types.
- [x] Finish host regressions, native browser ABI checks and package validation.
- [x] Generate/inspect screenshots and document the resulting controls/schema.
- [x] Commit the successful revision locally.
- [ ] Verify rich details, gallery, browser video handoff, genre persistence and
  mixed-feed update/DLC installation on PS4 11.00 + GoldHEN.


## Current implementation: r35 catalog description placement

- [x] Show the catalog description immediately after Version in Game Details.
- [x] Preserve local descriptions, existing wrapping and scroll navigation.
- [x] Run relevant host checks, build/validate the package and inspect screenshots.
- [x] Update expert documentation and commit the successful revision.
- [ ] Verify a refreshed catalog description appears in Details on PS4 for remote
  and matching installed titles, with scrolling for long text.

## Documentation: consumer guide and reference manuals

- [x] State the jailbroken-PS4 requirement and link the recommended JSON catalog server.
- [x] Shorten README to program description, setup and everyday consumer usage.
- [x] Move detailed configuration/recovery guidance to EXPERT.md.
- [x] Move build/deploy/testing/release guidance to DEVELOPMENT.md.
- [x] Show all supplied Screenshots/ captures as linked 30%-width thumbnails.
- [x] Validate links, gallery markup, JSON examples and unchanged package inputs.
- [x] Update progress/changelog and commit documentation without a package revision.

## Current implementation: r34 launch regression rollback

- [x] Record the reported r33 console crash when launching a game.
- [x] Restore the exact r32 native launch adapter and regression-test expectations.
- [x] Keep r32 verification fixes and the revised consumer/expert/development manuals.
- [x] Run launcher/uninstall/session regressions and build/validate r34.
- [x] Commit the successful revision and updated progress/changelog.
- [x] Game launching works after the r34 rollback; reported console launch crash
  resolved in the user's test (2026-10-17, PS4 11.00 + GoldHEN session).
- [x] Verify reopening r34 restores the saved selection after the game handoff.

## Current implementation: r32 verification and launch recovery

- [x] Inspect current queue/log evidence and external installed SFO through read-only FTP.
- [x] Confirm completed base packages with fresh installed Title ID despite catalog version differences.
- [x] Reconcile proven verification-timeout failures without downloading again.
- [x] Allow Installed launch while a released task only awaits verification.
- [x] Complete relevant host regressions, native build/validation and visual review.
- [x] Update documentation and commit the successful revision.
- [x] Retest r32 on PS4: Bonfire Peaks resolves to Completed/Installed on startup
  or refresh and X launches it; fresh installs and task cancellation still work.
- [ ] Retest an update whose installed version is below the expected update version;
  it must not be confirmed just because its base title exists.

## Current implementation: r28 reporting and queue actions

- [x] Restore r28 completion verification, progress labels and saved-history behavior.
- [x] Keep r29 wrapping, r30 menu scrollbars and Options-only Downloads/Collections.
- [x] Make Triangle cancel active/waiting/owned tasks or remove inactive history.
- [x] Confirm a stable package identity and retain records when saving fails.
- [x] Clear pending cancellation when a concurrent poll releases its task.
- [x] Complete host regressions, native build/validation, screenshot review and docs.
- [x] Commit the completed r31 change.
- [x] Retest r31 on PS4: cancel a waiting item, direct transfer and native task;
  confirm cancellation failure keeps ownership, retry works, and O keeps the task.
- [x] Retest r31: remove a historical Failed/Completed/Cancelled entry, restart,
  and confirm installed games/local records/saved packages remain intact.
- [x] Retest retained menu overflow, wrap boundaries and sidebar migration.
  Fresh r28 installs are already confirmed; no automatic history repair is included.

## Current implementation: wraparound navigation

- [x] Share Up/Down wrapping across all selectable menus and the sidebar.
- [x] Wrap Summary lists and tile rows while preserving sidebar access.
- [x] Cover menu boundaries, shrinking/empty lists, ragged grids and native stick input.
- [x] Complete host tests, native package validation, screenshots and local commit.
- [x] Retest r29 on PS4: Up at first / Down at last in menus, paginated pickers,
  source/profile lists, sidebar and both game layouts, including held-stick repeat.

## Current implementation: failed native-task recovery

- [x] Compare upstream native installer flow and duplicate-conflict handling.
- [x] Recheck and release only failed exact-content tasks identified by a local PKG.
- [x] Retry local registration once while preserving staging and new task ownership.
- [x] Complete host regressions, native build/package validation and local commit.
- [x] Reported installation failure resolved after r28 (user confirmation on
  PS4 11.00 + GoldHEN, 2026-10-16).
- [x] Separately verify r28's active-task preservation, resumed/changed task
  handling and cleanup-error recovery on hardware; inspect post-fix task logs.

## Current implementation: faster browsing and server profiles

- [x] L2/R2 page jumps and a letter picker for the current filtered view.
- [x] Remembered searches with controller selection and confirmed history clearing.
- [x] Named profiles for complete catalog source sets, with create/rename/delete and explicit Save.
- [x] Separate offline caches and rejection of results from previously selected sources.
- [x] Host tests, native package validation, screenshots, consumer docs and local commit for r27.

## Current implementation: navigation and maintenance

- [x] Sidebar Downloads, Collections and Updates shortcuts with correct back navigation.
- [x] Rating, last-launch, first-recorded-install, name and size sorting with both directions.
- [x] Updates view showing newer catalog versions for installed titles.
- [x] Base-game/version dependency checks at enqueue and immediately before installation.
- [x] Storage usage and confirmed cleanup of safely released staging/artwork files.
- [x] USB diagnostic export with credentials, URLs/tokens and personal notes excluded.
- [x] Host tests, PS4 package validation, screenshots, docs and local commit for r25.

## Current implementation: library and installer improvements

- [x] Persistent queue with distinct transfer, registration, installation and verification stages.
- [x] Bulk category/tag changes by Title ID with atomic save and cancellation.
- [x] USB backup/restore of personal records, including a pre-restore recovery copy.
- [x] Named saved collections with rating, launch-history and installation filters.
- [x] Base game/update/DLC grouping and installed/available version comparison.
- [x] Source setup helper with validation, counts and actionable connection/format errors.
- [x] Regression tests, PS4 package validation, screenshots and consumer docs for r23.
- [x] Refine/persist background-transfer phases, run full tests, validate r24 and refresh screenshots.

- [x] Remove Unity files and initialize independent repository.
- [x] Root Makefile, application identity, assets, and revision packaging.
- [x] Library metadata, installed discovery, settings persistence, and offline cache.
- [x] Tile/list dashboard, dialogs, controller navigation, splash, and About.
- [x] Artwork loading, background animation, desktop preview, and screenshots.
- [x] Restore the user's preferred r17 bubble style; retain animation toggle and motion checks.
- [x] Console URL keyboard, game launching, and installation monitoring.
- [x] Host tests, PS4 package build/validation, and consumer documentation.
- [x] Private `.env` FTP target configuration and `make deploy` with local FTP tests.
- [x] Settings installation-storage availability/free-space panel and manual refresh.
- [x] Fix startup asset loading across privilege setup; show the splash only while loading.
- [x] Route native shutdown through the PS4 shell after cleanup and credential restoration.
- [x] Draw PS4 controller button symbols next to action labels.
- [x] Request X confirm / O cancel for native dialogs in package metadata.
- [x] Resolve installed icons independently from title metadata, including Extended Storage.
- [x] Visible config-save confirmation/errors, draft snapshots and restart persistence tests.
- [x] Bind native HTTP policies to the same imported service initialized for downloads.
- [x] Canonical `/data/PS4-Game-Installer` storage and removal of inherited branding/imports.
- [x] Import the packed LncUtil launcher library from SystemService; validate the generated import table.
- [x] Resolve the entire HTTP client through one handle, including init and cleanup.
- [x] Left-stick/D-pad navigation with deadzone, repeat, dialog suppression and SDL hotplug tests.
- [x] Include patched (`gp`) game metadata in Installed discovery.
- [x] Add Internal / External USB selection using the PS4 Application Install
  Location preference, with connected-drive and registry readback checks.
- [x] Probe PKG URLs and stream a direct-download fallback for servers without
  range support; retain staging until BGFT releases the package.
- [x] Clean up failed/completed owned BGFT tasks before retrying; preserve active tasks.
- [x] Save both library selections before game launch and restore by title/URL after restart.
- [x] Add confirmed Installed > Uninstall through the native application service.
- [x] Store local Title ID ratings, installation history and launch request statistics separately from catalogs.
- [x] Show star ratings, Installed / Was installed badges and library-specific footer counts.
- [x] Replace the SDK's empty statvfs output with a native filesystem-capacity query.
- [x] Move HTTP transfer buffers to the heap and give native background workers explicit 512 KiB stacks.
- [x] Correct BGFT's system-visible staging path and submit validated PKG identity/size.
- [x] Reuse complete packages despite the SDK stat size mismatch.
- [x] Keep registration/task errors visible; distinguish downloading from installing.
- [x] Add local Title ID categories, labels, descriptions and tags, with drafts and explicit save results.
- [x] Create/rename/delete local categories/tags and browse/filter/group by them.
- [x] Show category/label on both layouts and selected local description previews.
- [x] Direct category/tag assignment from game actions, with existing-name pickers and new-name entry.
- [x] Remote artwork downloads work (user report for r16, 2026-10-16).
- [x] Application confirmed working on real PS4 11.00 + GoldHEN (user report, 2026-10-16).
- [ ] Complete the individual console acceptance checks below.

## Console acceptance checks

- [x] Retest r24: a PS4-managed remote transfer changes Downloading → Installing → Verifying → Completed; staged local copying stays Installing.
- [x] Retest r23: enqueue several packages, monitor stages, retry failures and restore queue state after closing/reopening.
- [ ] Retest r23: cancel a waiting transfer, a direct transfer and an owned BGFT task; cancellation failure keeps ownership/staging.
- [x] Retest r23: a completed transfer without the title/version stays unconfirmed; actual installation is confirmed by a fresh scan.
- [ ] Retest r23: export to ordinary USB, disconnect/reconnect, restore a valid backup and inspect the pre-restore recovery copy.
- [ ] Retest r23: bulk category/add/remove/clear tags in both libraries, including cancellation and failed-save retry.
- [ ] Retest r23: create/edit/delete saved rules, restart and verify rating, launch and installed-status filters.
- [ ] Retest r23: base/update/DLC grouping, version comparison, package details and O back navigation.
- [ ] Retest r23: source helper with a valid/empty catalog, HTML/404/unreachable source and PS4 IME; use/save only the tested URL.
- [x] Retest r22: Triangle > Assign category with X; Assign tags with X toggles / Square save, in both libraries.
- [ ] Retest r22: create names through IME, cancel with O, retry a failed write and scroll all game actions.
- [x] Retest r21: use PS4 IME for category, label, tags and description from both libraries.
- [x] Retest r21: save, refresh/restart and uninstall; personal metadata stays by Title ID.
- [x] Retest r21: browse/filter/group local categories and tags, including names with no assigned games.
- [x] Retest r21: rename/delete a category/tag; assignments update, notes/ratings/history remain.
- [ ] Retest r21: cancel an edit and retry a failed save without losing the draft.
- [x] Initial console launch reaches the splash and readable startup-error screen
  (user report, 2026-10-16; failed opening the icon in the previous build).
- [x] PS4 keyboard opens and correctly accepts source URL input (user confirmation,
  2026-10-16, PS4 11.00 + GoldHEN).
- [x] Saving the source URL/settings works on the console (user confirmation, 2026-10-16).
- [x] Console shows Installed game tiles and names (user report, 2026-10-16;
  icons were missing in the previous package).
- [x] Read-only FTP check confirmed a saved games URL before the data-directory
  correction (2026-10-16).
- [x] Retest r13: data moves to `/data/PS4-Game-Installer`, settings and caches are
  preserved, and About displays only the current application's identity.
- [x] Verify the saved source/settings survive app restart and library refresh,
  and the config-file confirmation is displayed.
- [x] r16 FTP log shows 44 packages loaded/refreshed and a successful connection
  test; user confirms remote artwork downloads work (2026-10-16).
- [x] Retest r27: L2/R2 pages both layouts without unintended repeats; Square on
  sidebar opens the letter picker, including filtered/grouped and empty views.
- [x] Retest r27: PS4 search keyboard cancellation, recent reuse, history clearing
  and restart persistence; O returns from the new dialogs correctly.
- [ ] Retest r27: create/rename/delete/switch profiles, explicit Save/discard,
  offline cache isolation and switching while a catalog request/transfer runs.
- [x] Retest r30: Downloads/Collections open from Options and O returns there;
  the three library sidebar entries wrap correctly. Retest Updates and selected
  game after launch/reopen; filters and both sort directions work.
- [ ] Retest r25: missing-base update/DLC is blocked; queued base completes and
  verifies before dependents; cancelling/failing the base explains dependent failure.
- [ ] Retest r25: staging/artwork sizes match the console; cleanup confirmation
  preserves active and unknown files, removes released/partial files, and refreshes usage.
- [ ] Retest r25: USB diagnostic export/readback, disconnect/read-only recovery,
  and absence of private URLs/credentials; report opens on a desktop.
- [x] Retest r10: Installed tiles, summary backgrounds and details show local icons
  for internal/external titles, including offline startup without a server.
- [x] Retest r9: loading ends without a forced splash delay.
- [x] Reported end-of-app CE-34878-0 resolved (user confirmation, 2026-10-16).
- [x] Separately verify closing startup/runtime error screens returns to the PS4
  shell without an error report; this path was not specified in the confirmation.
- [x] Splash, bubbles, controller repeat and both layouts render smoothly on the PS4.
- [x] Retest r19: restored r17 bubbles flow upward in Tiles and Summary;
  Settings > Background animation pauses/resumes them without affecting artwork.
- [x] Manipulator navigation works perfectly (user confirmation for r17, 2026-10-16).
- [x] Circle closes each window and Circle twice exits; Options opens its menu.
- [x] O cancels PS4 keyboard URL edits without changing the saved source.
- [x] Companion source loads real metadata (44 packages in r16 log) and remote
  artwork (user confirmation, 2026-10-16).
- [x] Internal and connected external titles appear with correct local metadata/sizes.
- [x] X launches games correctly in r17 (user confirmation, 2026-10-16).
- [x] r17 lists all installed games (user confirmation, 2026-10-16).
- [x] Retest r19: reopen after launching a game; same library/title/search restored.
- [x] Retest r19: rate 1–5 stars, refresh/restart and confirm ratings remain by Title ID.
- [x] Uninstall removes games successfully (user confirmation and FTP log, 2026-10-16).
- [x] Separately verify Uninstall cancellation, Was installed, ratings and history
  after restart; those outcomes were not specified in the confirmation.
- [x] Retest r19: Available footer shows the installable package count when switching libraries.
- [x] Package installation works after r28 (user confirmation, 2026-10-16).
- [ ] A submitted installation task survives closing the application.
- [x] r19 direct download reaches completion on hardware without the reported crash
  in the observed Back to Bed attempts (user report and FTP log, 2026-10-16).
- [x] Resolve the reported local-registration refusal 0x80990086 with failed
  task 268435469 / 0x80991404; user confirms installation works after r28
  (2026-10-16). No post-fix task log was collected for internal-state verification.
- [x] Retest r20: Retry reuses the complete staged file, and a persistent dialog
  shows an actual registration error if the system still refuses the request.
- [ ] Storage panel detects a PS4 Extended Storage drive and updates after disconnect.
- [x] Install to each PS4 system destination and verify the resulting game location.
- [x] Retest r17: X in Installation destination changes the selected PS4 default;
  disconnect Extended Storage and verify the unavailable drive cannot be selected.
- [x] Continue large-package/repeated-transfer stability checks after r19's stack repair;
  no r17 crash dump was available to establish its exact fault.
- [x] Retest r19: internal/external capacities are correct, including disconnect and full/unknown states.
- [x] Clear failed r16 downloads once, then install through the direct fallback,
  observe progress, retry an error, and verify staging cleanup.
- [x] Returning/restarting refreshes installed state and reconciles tracked tasks.
- [x] Offline startup retains Installed and the cached remote library.
- [ ] Missing/corrupt artwork, unavailable storage and connection failures recover.
