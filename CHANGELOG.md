# Changelog

## r47 — Download recovery and serial installation — 2026-10-19

- Tolerate longer package-server stalls and retry transient download interruptions.
- Keep partial downloads for retry; verify saved bytes before appending so servers
  ignoring Range cannot corrupt a package. Reuse complete downloaded packages.
- Download packages to staging, then install one at a time. Show Waiting to install
  for completed downloads, preserve waiters across restart and retain cancellation.
- Console logs identify ARK's reported failure as a staging timeout. Host/native
  regressions, all 19 Python integrations, package validation and queue visual
  review passed. Deployed r47; full ARK and sequential-installation hardware
  acceptance remain pending.

## r46 — Internet Archive sign-in — 2026-10-19

- Add a masked Archive.org account sign-in/sign-out screen for downloads that
  require an account with access to the file.
- Save login cookies in a separate private console session file and attach them
  only to verified HTTPS Archive.org package requests, including CDN redirects.
- Reuse existing package staging/local installation; keep credentials out of
  catalogs, queue records, backups and diagnostics. Passwords are not saved.
- Full host/native suite and 19 Python integrations passed, with UI captures and
  package validation. Console account login and full installation remain pending.

## r45 — GoDaddy HTTPS chain compatibility — 2026-10-19

- Load GoDaddy's exact published G2 issuing certificate alongside the unchanged
  Mozilla roots, addressing the documented legacy cross-signed-chain limitation.
- Pin the asset, verify its signature/CA constraints and package it with provenance.
  Catalog/package certificate checks remain enabled; no trust bypass is added.
- Firmware 9.00 hardware comparison: r44 fails unknown CA; the same request on r45
  reaches HTTP 401 from the file host. Desktop probes confirm upstream 401/500
  responses. TLS is working for that request; full installation remains unverified.

## r44 — PS4 HTTPS compatibility follow-up — 2026-10-19

- Select the native keepalive connection mode documented to retain custom HTTPS
  trust. Keep the existing CA bundle and certificate checks unchanged.
- Show the failing hostname and redirect count on send errors, without exposing
  signed paths/queries. Describe unknown-CA errors as certificate trust failures.
- Record SSL/verification errors, errno and CA-load metadata through the existing
  app.log error path, preserving the original failure if diagnostics are unavailable.
- Replace the misleading download sentence shown for every installation error.
- r42 failed verified package HTTPS on a user's PS4 9.00; r44 needs a console
  retest and is not yet a hardware-confirmed fix.

## r42 — HTTPS compatibility — 2026-10-19

- Load a pinned Mozilla CA bundle for verified native HTTPS catalogs/packages.
- Follow up to five HTTP(S) redirects while rejecting HTTPS downgrades.
- Stage HTTPS packages through the app before local installation; keep remote
  BGFT installation for resolved HTTP URLs with Range support.
- Retain catalog URL identity and complete staging reuse across redirects/retries.
- Full host suite (including 18 Python tests), native HTTP regressions and PS4
  package validation passed. Hardware acceptance on PS4 11.00 + GoldHEN remains
  pending.

## Release documentation — 2026-10-18

- Prepare concise r40 release highlights in a temporary, Git-ignored notes file.
- User confirms r40 is released and the temporary notes have been removed.


## r40 — 2026-10-18

- Add Show screenshots immediately after Details in game Actions when images are
  supplied. Open the existing gallery directly and return to Actions with O.
- UI/metadata regressions and native package validation passed; console shortcut
  acceptance remains pending on PS4 11.00 + GoldHEN.


## r39 — 2026-10-18

- Reduce the selected title/category scrolling pause from 1.25 to 0.35 seconds.
  Keep the scrolling speed and clipping unchanged.
- The ASan/UBSan pixel regression confirms motion by 0.9 seconds. Renderer
  animation checks and native package validation passed; PS4 timing needs retesting.


## r38 — 2026-10-18

- Show actual screenshot failures and retry instructions instead of an endless
  Loading image message. Record bounded artwork outcomes in app.log by cache key.
- Add a console HTTPS compatibility path limited to public IGDB artwork. Catalogs,
  packages and other artwork origins keep their existing certificate verification.
- Complete host suite (including 15 Python tests), native package validation and
  desktop gallery/error capture review passed. Actual PS4 screenshot display
  remains pending hardware acceptance.


## r37 — 2026-10-18

- Avoid the OpenOrbis allocator shrink path in JSON output after a kernel-log
  investigation of r36 refresh/startup crashes. Add large-JSON allocator regression
  coverage and startup/refresh phase logging; preserve existing saved data.
- Remove local labels from records, editing, searching and every game layout.
  Legacy label keys are ignored while other personal information is retained.
- Give categories the full row; scroll overflowing selected title/category text
  inside its tile. Separate Genres and manually assigned Tags in Details, followed
  by game modes, offline/online player counts and split-screen support.
- Full host suite (including the allocator regression and 15 Python tests) and
  OpenOrbis package validation passed. PS4 startup/refresh and UI acceptance
  remain pending on PS4 11.00 + GoldHEN.

## r36 — 2026-10-18

- Rework Game Details with ordered bold keys, one personal-or-remote description,
  collapsed Details/Tech. details and readable package URLs.
- Read/cache the embedded IGDB tree: publisher, release, plot, genres/themes,
  user ratings, languages, multiplayer and other supplied metadata. Keep local
  descriptions, categories, ratings and tags separate from server updates.
- Add an in-app screenshot gallery with full-size caching and video links through
  the PS4 browser. Save library position before browser handoff.
- Expand Available into remembered genre filters with wraparound and scrollbars.
  Explicit package_type routes updates/DLC to Updates, including mixed feeds and
  queue restart/install-slot handling. Existing game-launch flags are unchanged.
- Full host suite (including 15 Python tests) and OpenOrbis package validation
  passed. New UI/media and mixed-feed installation
  acceptance remains pending on PS4 11.00 + GoldHEN.

## r35 — 2026-10-17

- Show the remote catalog's description directly after Version in Game Details,
  instead of after all other metadata. Keep local notes separate and omit the
  catalog description row when the source supplies no text.
- Model regressions, native package validation and desktop screenshot review
  passed. PS4 display acceptance is pending; native launch/installation is unchanged.

## r34 — 2026-10-17

- Revert r33's skip-launch-check flag after the user reported a console crash on
  game launch. Restore the exact r32 launcher and native test expectations, using
  only the original skip-system-update flag and the normal PS4 close confirmation.
- Keep r32 verification/launch-access fixes, queue actions and the documentation
  reorganization with its screenshot gallery.
- Native launcher/uninstall and App regressions passed; r34 package built and
  validated. User confirmed launching works after the rollback (2026-10-17,
  PS4 11.00 + GoldHEN session).

## Documentation — 2026-10-17

- State explicitly that the app works only on a jailbroken PS4 and recommend the
  companion catalog server for generating/hosting the JSON game library.
- Reduce README from 793 to 114 lines, keeping program description, setup and
  everyday consumer instructions.
- Add EXPERT.md for catalog hosting, configuration, profiles, organization,
  storage, downloads, data recovery and troubleshooting.
- Add DEVELOPMENT.md for Linux/Docker builds, deployment, testing, package
  revisions, PS4 integration and release procedures.
- Include all 11 supplied PS4 captures from Screenshots/ as linked 30%-width
  README thumbnails. Preserve the original image files.
- Validate local links/anchors, JSON examples and gallery markup. Documentation
  only; the successful application revision remains r33.

## r33 — 2026-10-17 (withdrawn)

- Request direct game handoff with the SDK skip-launch-check flag, retaining the
  existing system-update-check bypass. Intended to suppress the PS4 prompt asking
  to close this installer; console acceptance is pending.
- Preserve session/launch-record saving before handoff and error handling if launch
  fails. No queue, download, installer or renderer changes.
- Native launch/uninstall and App session regressions passed; r33 package built
  and validated. Subsequent hardware feedback reports a console crash on launch;
  the prompt bypass is reverted in r34.

## r32 — 2026-10-17

- Fix completed base-game installations timing out when the catalog version differs
  from the actual installed package. Require native completion and a fresh installed
  Title ID match; keep update-version and missing-title checks.
- Recheck verification-timeout failures with saved native completion against fresh
  installed evidence, recovering confirmed installs without another download.
- Allow Installed games to launch when the package task has been released and only
  verification remains. Keep active/tracked task and duplicate-install guards.
- Retain r28 progress labels, r31 cancellation actions and native installer code.
- Feature/UI/App regression suites, native package validation and screenshot review
  passed. Updated consumer documentation; console acceptance of r32 remains pending.

## r31 — 2026-10-17

- Restore r28 queue reporting and verification; preserve historical results instead
  of automatically reclassifying them. Resume temporary r30 unverified records
  through the original verifier. Native download/installation code is unchanged.
- Fix the Triangle queue action: cancel active/waiting/owned tasks, or remove
  inactive history without touching installed games, local records or saved packages.
- Confirm actions against the selected package identity, preserve history on save
  failure, and clear pending cancellation if a concurrent poll releases the task.
- Keep menu wrapping, overflow indicators and Downloads/Collections in Options.
- Host/native-service regressions and all 15 Python integrations passed. Built
  and validated `PS4Game_installer-r31.pkg`; console cancellation acceptance
  remains pending. Screenshot review and consumer documentation are updated.

## r30 — 2026-10-16

- Add scrollbar thumbs and directional overflow arrows to paginated menus,
  including additional sources and scrollable progress/details dialogs.
- Keep Downloads and Collections in Options; remove their sidebar shortcuts.
  Retain Installed, Available and Updates, with migration of old saved positions.
- Preserve native PS4 installation success as Completed despite catalog/version
  or library-scan discrepancies. Repair old verification-timeout records on load;
  distinguish genuinely unverified outcomes from installation failures.
- Show completed/released task tracking clearly and omit transfer percentages
  from completed queue rows. Keep real errors and owned-task cleanup checks.
- User confirmed uninstall works on PS4 11.00 + GoldHEN (2026-10-16).
- Clarification: the reported Failed Data records were historical; fresh r28
  installs are confirmed working. Native installer/download code remains unchanged
  from r28, while r30's queue completion policy still needs console acceptance.
- Host/native regressions, package validation and screenshot review passed.
  New r30 behavior still needs console acceptance; see PROGRESS.md and TODO.md.

## r29 — 2026-10-16

- Wrap Up/Down selection between the first and last entries in all menus,
  including sidebar, options, settings, actions, ratings, categories/tags,
  collections, downloads, USB pickers, source setup, letters and server profiles.
- Wrap Summary game lists and top/bottom tile rows, keeping the column where
  possible across short/grouped rows. Keep the sidebar reachable with Left.
- Handle empty, single-entry and changing lists without invalid selections;
  retain primary-source protection in the additional-source picker.
- Host/native regressions, PS4 package validation and desktop screenshot review
  passed. Console acceptance of wraparound navigation remains pending.

## r28 — 2026-10-16

- Recover local installation blocked by an old failed BGFT task, including the
  reported 0x80990086 / 0x80991404 case. Identify it from the validated PKG Content
  ID/subtype, recheck its identity/error, release it and retry registration once.
- Keep active/unknown tasks and installed titles; preserve the downloaded package
  on refusal and report cleanup errors. Preserve ownership of a newly registered
  task when starting it and cleanup both fail. Host regressions and native package
  validation passed. User confirmed the reported installation failure is resolved
  on PS4 11.00 + GoldHEN after r28 (2026-10-16); other task-state checks remain open.

## Console installation investigation — 2026-10-16

- Confirm from fresh console logs that the complete Back to Bed package is
  already saved locally and reused. Installation is refused with 0x80990086;
  exact Content ID lookup reports existing failed task 268435469 / 0x80991404.
  Record the unresolved native-task conflict and manual recovery check without
  deleting console tasks or rebuilding the application.

## r27 — 2026-10-16

- Add L2/R2 page jumps in both layouts, native/SDL trigger bindings and desktop
  Page Up/Page Down keys. Add a letter picker from Options or Square on the sidebar,
  with counts from the current filtered view and preserved sort/group settings.
- Add a search menu with 12 recent terms, deduplication, immediate reuse, clear
  current search and confirmed history clearing. Persist history in session.json
  independently of personal records and report session-write failures.
- Add 16 named server profiles storing complete source sets, with save-copy,
  rename, confirmed delete and explicit Settings Save/discard. Keep existing
  settings compatible, preserve active-profile URL edits and show the active name.
- Keep separate caches per source set, migrate the legacy cache for an existing
  setup, and reject old in-flight catalog results after switching sources. Preserve
  queued tasks and local Title ID records while clearing stale remote enrichment.
- Host/native validation and screenshots are tracked in PROGRESS.md. New controller
  bindings/profile behavior still need console acceptance.

## r25 — 2026-10-16

- Add sidebar Downloads, Collections and Updates with contextual back navigation
  and saved Updates/sidebar state. Show newer patches for installed Title IDs and
  installed-to-available versions in both layouts.
- Add rating, last launch here and first recorded install sorting alongside name
  and size, with ascending/descending order, unknown values last and migration of
  existing size-sort preferences. Paginate expanded Settings.
- Check update/DLC base-game requirements before enqueue, on Retry and against a
  fresh console scan immediately before network/registration work. Persist queued
  base dependencies, wait for verified installation and explain failed dependencies.
  Support optional catalog `required_base_version`; firmware remains advisory.
- Add confirmed staging/artwork cleanup and measured cache/staging/protected usage.
  Preserve active jobs, unknown legacy packages and ambiguous ownership via durable
  staging leases. Retain task IDs and the Failed state when a failed start cannot
  unregister its task; allow explicit cancellation without losing file ownership.
- Add USB diagnostic reports containing allowlisted counts, stages, Title IDs and
  selected error codes. Exclude raw logs, URLs, credentials and personal records;
  recheck the drive, use unique files and verify the written report.
- Package checksum verification was excluded as requested. New console flows still
  require hardware acceptance; host/native validation is recorded in PROGRESS.md.

## r24 — 2026-10-16

- Distinguish PS4-managed background downloads from direct staging and local
  installation copies. Show Downloading while background bytes are arriving,
  Installing after transfer, then verification; persist transfer mode and phase.
- Keep completed/verified stages stable while awaiting system task release.
  Preserve existing queue files whose transfer mode is unknown.

## r23 — 2026-10-16

- Add a persistent 200-item download queue with waiting, checking, downloading,
  registration, installation, verification, completion, failure and cancellation
  stages. Show transfer bytes/rate/estimated time, diagnostics, retry, confirmed
  cancellation of owned tasks and finished-history cleanup.
- Verify base-title/update presence through fresh installed scans. Report an
  unconfirmed installation after two minutes instead of treating transfer 100%
  or a vanished task as success. Keep DLC/system completion distinct from tiles.
- Add bulk category assignment and add/remove/clear tags with Title ID selection,
  all-or-nothing writes, cancel and failed-save retry.
- Add 64 named saved collections combining search, category/tag, star ranges,
  launch history and installation rules. Read old profiles and write schema 3.
- Add USB profile export/readback and confirmed validated restore, retaining a
  pre-restore recovery copy. Include personal records and saved collections.
- Add related base/update/DLC views, Title ID grouping and numeric version
  comparison. Base-game presence no longer marks its DLC installed.
- Add a source setup helper with staged URLs, catalog validation, counts, missing
  ID warnings, HTML/HTTP/format diagnostics and explicit use/save.
- Native compilation and host validation do not establish PS4 hardware acceptance
  for the new queue, installation verification, task cancellation or USB flows.

## r22 — 2026-10-16

- Add Triangle > Assign category and Assign tags in both libraries. Reuse an
  existing name or enter a new one directly, without the full local-info editor.
- X saves a category; X toggles tags and Square saves them together. O cancels.
  Failed saves retain the picker and draft for retry, with a visible error.
- Focus the current assignment on entry and newly entered names after typing.
  Paginate game actions so every action, including Uninstall, remains reachable.

## r21 — 2026-10-16

- Add local category, label, description and up to 16 tags per Title ID through
  Triangle > Edit local info in Installed and Available, with explicit Save and
  discard handling. Failed saves keep the draft for retry.
- Always show category/label on tiles and summary rows; show the selected game's
  local description preview and include the full local info in Details/search.
- Browse/filter by local category or tag from Options. Create, rename and delete
  local names, including names not assigned to a game yet. Renames update game
  assignments; deletion keeps games, ratings, notes and history.
- Add local-category and multi-tag grouping, unique-game counts and persisted
  filters/selection within tag groups. Keep catalog package types independent.
- Read existing player records and write schema 2 atomically. Older app versions
  preserve this file rather than stripping fields they do not understand.

## r20 — 2026-10-16

- Correct local installation paths for BGFT's system namespace: translate this
  app's `/data/...` staging paths to `/user/data/...`.
- Validate the downloaded PKG header, Content ID, Title ID and actual 64-bit file
  size before registration; submit the real package identity and length.
- Reuse complete staged downloads using descriptor seeking instead of the SDK's
  incompatible stat size field. Retrying a registration does not download again.
- Distinguish package-download progress from PS4 installation. Show registration
  and task errors in a persistent dialog with Retry and Close.
- Diagnose duplicate-content errors by looking up the matching BGFT task, retaining
  the error code and package. Never cancel unrelated tasks or remove installed games.
- Console evidence: r19 downloaded Back to Bed completely but BGFT rejected
  registration with `0x80990086`; no installation occurred. New registration behavior
  still requires a hardware retest.

## r19 — 2026-10-16

- Save library, search and both selected games before launching; restore by Title
  ID/package URL when reopening, including after catalog reordering.
- Add Installed > Triangle > Uninstall with confirmation; preserve local history.
- Add local 1–5 star ratings and installation/launch records keyed by Title ID in
  `player.json`, independent of remote catalogs. Show stars and Was installed.
- Keep the footer count specific to the active library, including Available's
  installable package count.
- Replace the SDK's non-populating statvfs wrapper with a native capacity query;
  distinguish an unreadable capacity from a full drive.
- Remove large HTTP stack buffers and provide explicit 512 KiB native worker
  stacks. Add download-stage logging. These address a stack-overflow hazard in
  the reported download-crash path; the exact CE-34878-0 dump is not yet available.
- Restore the softer r17 bubbles requested by the user.
- User confirmed r17 launching, controller navigation and complete Installed
  discovery work on PS4 11.00 + GoldHEN. New behavior needs console acceptance.

## r18 — 2026-10-16

- Make dashboard bubbles visibly brighter, with soft rims, highlights and faster
  upward movement. Cache the six bubble sprites instead of recomputing their
  shading every frame.
- Draw bubbles above summary artwork and its dimmer, behind game cards and
  controls, so they remain visible in both dashboard layouts.
- Preserve the Background animation setting, including its Settings preview.
  Add full-renderer checks for visible motion, artwork layering and paused frames,
  plus paired dashboard captures for inspecting movement.

## r17 — 2026-10-16

- Include patched-game (`gp`) app metadata in Installed. The previous filter hid
  GTA V, Minecraft, DOOM, DIRT 5 and other updated titles on the reported console.
- Link the packed LncUtil launcher library to SystemService instead of resolving
  it through default-library dlsym, which failed with `0x80020003`. Validate the
  generated Orbis ELF's library/module imports during packaging.
- Replace the informational storage panel with an Internal / External USB selector
  that changes the PS4 Application Install Location preference. Check the drive,
  reject unknown values, verify writes and restore the previous value on mismatch.
- Check package URLs and signatures before BGFT registration. Servers without
  Range support use bounded-memory direct downloads to internal staging, followed
  by local BGFT installation. Show progress and handle cancellation, short files,
  invalid responses, missing content lengths and insufficient staging space.
- Let BGFT read package sizes from metadata. Retire only failed or fully completed
  owned tasks; keep active tasks and staging files when completion is uncertain.
- User confirmed remote artwork downloads work in r16. The new launch,
  destination selector and installation paths still need console acceptance.

## Console confirmation — 2026-10-16

- User confirmed the application works on real PS4 11.00 + GoldHEN hardware.
- User confirmed source URL entry through the PS4 keyboard and saving settings
  both work on the console.
- User confirmed the reported end-of-app CE-34878-0 crash is fixed. The exact
  installed revision and exit path were not specified. Documentation/tracker
  update only; no package revision increment.

## r16 — 2026-10-16

- Added left analog stick navigation to the PS4 app and desktop preview for games,
  sidebar, menus, settings and details. Previously only the D-pad was read.
- Filter stick drift with separate activation/release thresholds, stabilize
  diagonal movement and repeat held directions after a short delay. D-pad input
  takes priority; action buttons still emit once per press.
- Suppress held controls after closing the keyboard until buttons are released
  and the stick is centered. Handle controller disconnect/reconnect in the preview.
- Added native pad-sample, navigation-state and SDL virtual-controller coverage.

## r15 — 2026-10-16

- Use one explicitly resolved HTTP module for initialization, templates, request
  settings, transfers and cleanup, addressing the recurring pre-request
  `Disable redirects (0x80431001)` report. Removed the linked HTTP stub dependency.
- Keep redirect rejection, certificate verification, timeouts and download limits
  enabled. Errors from request setup still stop the transfer and preserve cache.
- Native tests now provide HTTP functions only through the module resolver, so
  accidentally mixing linked HTTP calls into the adapter fails to link. Added
  missing-module/symbol, null-function and retry coverage.

## r14 — 2026-10-16

- Fixed the reported `Load game launcher: 0x80020002` failure by resolving LncUtil
  exports from `libSceSystemService.sprx`, their actual containing module.
- Check launcher initialization errors, accept its already-initialized result,
  reject a missing foreground user and retain retry after a failed module load.
- Added native-adapter tests for the module lookup, launch parameters, positive
  app IDs, failed calls and retry. A successful PS4 launch still needs retesting.

## r13 — 2026-10-16

- Standardized console storage on `/data/PS4-Game-Installer` for settings, catalog,
  tasks, artwork and logs. This app's existing data is moved there on first launch
  when the destination does not exist.
- Removed the unrelated application importer and inherited branding/repository
  references from application sources, About, README and packaged credits.
- Added data-directory tests for preservation, repeated launches, destination
  precedence, fresh installs and blocked paths.

## r12 — 2026-10-16

- Kept Settings open after saving, with a visible config-file confirmation, inline
  connection results and write errors. Accepting the keyboard is explicitly labeled
  as a draft; failed writes preserve the draft for retry.
- Save commands capture the selected settings when pressed. Acknowledgement follows
  writing and checking the resulting `config.json`; successful saves are logged.
- Corrected native HTTP/SSL bindings to use the initialized imports rather than
  loading extra service modules for redirect/time-out and SSL cleanup functions.
  This addresses the console-reported `Disable redirects (0x80431001)` failure.
- Added real App/UI persistence tests covering restart and failed-save retry, and
  native HTTP adapter tests for initialization, policies, streaming and cleanup.

## r10 — 2026-10-16

- Fixed installed-title icon lookup: read public `/user/appmeta` artwork separately
  from `/system_data/priv/appmeta` title metadata, including both external subtrees.
- Keep a fallback for layouts with icons beside their metadata, skip empty or
  unreadable icon files, and retain titles whose artwork is missing.
- Added an offline discovery-to-image-loading regression for internal and external
  titles, duplicate metadata, missing artwork and co-located icon fallback.

## r9 — 2026-10-16

- Removed the artificial three-second splash minimum. Startup shows the dashboard
  as soon as the initial library scan finishes, and errors appear without a delay.
- Replaced written controller button names with drawn PS4 button symbols and
  keycaps alongside action labels throughout the dashboard and dialogs.
- Corrected package metadata to request X confirm / O cancel for the native PS4
  keyboard, matching application controls. Previously O selected characters and
  X closed the keyboard. The system keyboard is retained.
- Updated keyboard guidance and added a regression that generates and reads real
  SFO metadata to check the common-dialog button assignment.

## r8 — 2026-10-16

- Changed native termination to request exit through the PS4 system service after
  application resources and worker threads have been cleaned up. Both normal exit
  and closing an error screen use this path instead of returning through libc.
- Restore the original process credentials and sandbox directories before the exit
  request, including recovery after a partially failed privilege setup.
- Added native lifecycle regressions for successful shutdown, startup/restore
  failures and an unsuccessful shell exit request. The user subsequently confirmed
  the reported end-of-app CE-34878-0 crash is fixed (2026-10-16).

## r7 — 2026-10-16

- Fixed the reported startup failure opening `/app0/assets/icon.png`: load bundled
  artwork before homebrew privilege setup changes the process filesystem root.
- Retained the bundled font in memory so later glyph loading does not depend on
  sandbox asset paths after privilege setup.
- Kept the splash visible for at least three seconds from its first presented
  frame, including startup failures and early controller input.
- Added real-application startup regressions that hide the asset directory at the
  privilege boundary and check both successful and failed startup timing.

## r6 — 2026-10-15

- Added a Settings installation-storage panel with internal/PS4 Extended Storage
  availability, free space and manual refresh after USB connection changes.
- Added guidance for the PS4's Application Install Location setting. In-app
  destination selection is still pending verification of the native control API;
  the panel does not change the installation destination.
- Added storage mount checks, controller coverage and connected/disconnected USB
  desktop captures.

## r4 — 2026-10-15

- Added `make deploy` to build, verify and upload the current package to the FTP
  target configured in the private `.env` file.
- Added an example target configuration and consumer deployment instructions.
- Excluded private environment files from Docker build contexts as well as Git.
- Added local FTP integration tests for uploads, failed transfers, remote size
  checks, package integrity and configuration parsing without shell evaluation.

## r3 — 2026-10-15

Native PS4 Games Installer redesign (hardware validation pending).

- Replaced the inherited repository with an independent native OpenOrbis project.
- Moved application sources to `src/` and builds to the root Makefile.
- Added new application identity, original SVG icon, and licensed Noto Sans font.
- Added persistent, input-based package revisions and Docker-managed tools.
- Added settings persistence/migration, full catalog details and installed-title matching.
- Added installed metadata discovery, game launching and BGFT progress adapters.
- Added tile and summary views, installed/available libraries, grouping, and controller menus.
- Added graphical source configuration, game details, splash, About and Circle exit confirmation.
- Added animated bubbles, asynchronous cover caching, offline libraries and task tracking.
- Added desktop preview, sample covers, screenshots and controller navigation tests.

- Added transaction tests for failed/concurrent builds and input changes while packaging.
- Added artwork/Unicode/clipping checks under AddressSanitizer and UBSan.
- Added 50,000-entry library navigation coverage and reusable screenshot captures.
- Resolved launcher functions from LncUtil and loaded private installed-title services after privilege setup.
- Bundled dependency notices and added consumer setup, server integration and troubleshooting docs.
- Verified unchanged builds retain their revision; r3 supersedes the intermediate r2 package.
