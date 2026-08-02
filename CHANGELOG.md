# Changelog

## [0.3.1](https://github.com/brunofgmag/fs-organizer/compare/v0.3.0...v0.3.1) (2026-08-02)


### Bug Fixes

* **view:** give value_or a typed fallback for GCC ([48a0bf0](https://github.com/brunofgmag/fs-organizer/commit/48a0bf01c84888f5a5eabfa966351134b51b98fc))
* **view:** give value_or a typed fallback for GCC ([77505b3](https://github.com/brunofgmag/fs-organizer/commit/77505b399b89dbc5e5f172f16f6ebe96ba8a7def))

## [0.3.0](https://github.com/brunofgmag/fs-organizer/compare/v0.2.0...v0.3.0) (2026-08-02)


### Features

* **i18n:** turn the source strings English ([23aa459](https://github.com/brunofgmag/fs-organizer/commit/23aa459efb27c133c9a79cd1c6bb838c49553767))
* **tools:** let fsorg-shot render either language ([22cc113](https://github.com/brunofgmag/fs-organizer/commit/22cc1131e61a814f9ce909e0f5cbd04c78d9d6e9))
* **view:** drop the appearance note, credit the author, and unify the triage label ([a8123b8](https://github.com/brunofgmag/fs-organizer/commit/a8123b8313d1c12bd6b1126c845651c86f81bb42))
* **view:** switch the language without a restart ([3b02962](https://github.com/brunofgmag/fs-organizer/commit/3b02962f26d785da766f0ef45f31a1bc5533c1c1))


### Bug Fixes

* **view:** close the two-axis review of the English inversion ([f11d32a](https://github.com/brunofgmag/fs-organizer/commit/f11d32aa5941cc305431314007bef482a2903641))
* **viewmodel:** pair layoutChanged with layoutAboutToBeChanged ([432512f](https://github.com/brunofgmag/fs-organizer/commit/432512fa3ceab8a36883d4f461ce1b22c5d79fb3))

## [0.2.0](https://github.com/brunofgmag/fs-organizer/compare/v0.1.0...v0.2.0) (2026-08-02)


### Features

* **app:** give presets a service that owns their crud and their application ([3cff5e3](https://github.com/brunofgmag/fs-organizer/commit/3cff5e37e93d8fc350a3eef79cac1acf511d6abd))
* **app:** keep every preset in a file of its own, under the profile that owns it ([bd18a1c](https://github.com/brunofgmag/fs-organizer/commit/bd18a1cf5e28d8aaf7946a7dff02f7b522b8da4a))
* **app:** let the library organizer create, rename, remove and move on the library it owns ([8462436](https://github.com/brunofgmag/fs-organizer/commit/8462436c31f0b7f85f1e0bf83ad69e062e3b8a99))
* **application:** import the legacy libraries in one pass ([985b919](https://github.com/brunofgmag/fs-organizer/commit/985b919606168c58c15ef2a90e4bbade2328e109))
* **application:** let the session drop a library and repoint a destination ([ec478a7](https://github.com/brunofgmag/fs-organizer/commit/ec478a7c13af198cad9cde7cce2766f84e557611))
* **app:** wire the tabs, the triage strip and the footer of every screen ([8fb337b](https://github.com/brunofgmag/fs-organizer/commit/8fb337bc2792e01cc2e69a86a27c7e863ca35218))
* **community:** show the destination truth with repair behind a sidebar ([8512ece](https://github.com/brunofgmag/fs-organizer/commit/8512ece988f31afe398961c299313bcf80d349a0))
* **domain:** count what a preset holds and where it lands ([a6d2414](https://github.com/brunofgmag/fs-organizer/commit/a6d2414299f55e7676050c906f60e58185d84fc5))
* **domain:** edit a profile without leaving an override pointing nowhere ([944829b](https://github.com/brunofgmag/fs-organizer/commit/944829b5affa7d37f1aa5f3cdad5a8d8e52b4a5a))
* **domain:** read a legacy preset against the tree that is there now ([79948e3](https://github.com/brunofgmag/fs-organizer/commit/79948e3cdb51defd14c7c81c39ba8dd136677b32))
* **domain:** tell a declared category from a leftover folder, and name who each lookup ignores ([9211878](https://github.com/brunofgmag/fs-organizer/commit/9211878ccde5e5a2a9dbd6f9a85c6ce59a6cf105))
* **import:** add the Windows adapter, the link step and the import service ([c57d76b](https://github.com/brunofgmag/fs-organizer/commit/c57d76b705aad0c3a354d4837928ef5a4f9d9b3c))
* **import:** import physical addons, quarantine the loser, keep a journal ([df494dc](https://github.com/brunofgmag/fs-organizer/commit/df494dc81af562633f5e695b84bc3c025eefdcc1))
* **import:** precheck the free space and survive an interrupted copy ([ab4d436](https://github.com/brunofgmag/fs-organizer/commit/ab4d4361f3d1e01ba90c47a394cc2136818f9ccb))
* **import:** verify the copy and guard what may be removed ([2ad803f](https://github.com/brunofgmag/fs-organizer/commit/2ad803fd058aa1c9f1cdfdd5bfdcc0bf697e742c))
* **infra:** add the Win32 adapters and the fsorg-probe tool ([8b5f68b](https://github.com/brunofgmag/fs-organizer/commit/8b5f68b934641f600f11c332fbf854a891f03091))
* **infrastructure:** check GitHub Releases and stage the update ([4e438c9](https://github.com/brunofgmag/fs-organizer/commit/4e438c9a9fadc90a4bfc768e33913f496d3326cc))
* **infrastructure:** find the old program and read what it saved ([1ec59fb](https://github.com/brunofgmag/fs-organizer/commit/1ec59fb174e3e3be57a97bfb95e376f5eb49d1f0))
* **journal:** record every step of an import as its own entry ([f4aace6](https://github.com/brunofgmag/fs-organizer/commit/f4aace662de5acfb47bddd63cbd68c58694acdff))
* **journal:** record the quarantine and let an import drop the undo batch ([fc9557e](https://github.com/brunofgmag/fs-organizer/commit/fc9557e6944a2e07b1eb27296060ee71da16aecd))
* **link:** add the domain link engine with its destructive guards ([d0bbbf9](https://github.com/brunofgmag/fs-organizer/commit/d0bbbf925d1c02c074ae82de1df9f9d9ab1ffbac))
* **model:** give a preset a name, entries and the action each entry carries ([f1929b2](https://github.com/brunofgmag/fs-organizer/commit/f1929b2cbf695b8cbb1ef75a9d317eae75bf1d5f))
* **preset:** let the listing say when each preset was last written ([2a79937](https://github.com/brunofgmag/fs-organizer/commit/2a79937a66f1e59c655a6a67518d97981db39002))
* **preset:** plan what a mode would do to the enabled set, and capture that set back ([5fe577b](https://github.com/brunofgmag/fs-organizer/commit/5fe577badea358532e8cd31c49c61986c629e571))
* **repair:** plan broken-link repairs and run them with undo ([7a32f68](https://github.com/brunofgmag/fs-organizer/commit/7a32f687d2b25b3c400536565eae7f405531b860))
* **settings:** let the settings file carry the link type and the hash check ([b43d4a6](https://github.com/brunofgmag/fs-organizer/commit/b43d4a6a571e51bc21b91b98e8c1e08a76935ba0))
* **setup:** add settings persistence, the setup wizard and the app shell ([b83dd9e](https://github.com/brunofgmag/fs-organizer/commit/b83dd9ed20dcde2bcabe76302551480875eecffe))
* **state:** classify destination entries and resolve enabled addons ([da09378](https://github.com/brunofgmag/fs-organizer/commit/da09378e884769be32792cf8c311be86177cc94a))
* **tools:** let a screen be looked at without driving the mouse ([784c4e1](https://github.com/brunofgmag/fs-organizer/commit/784c4e11b33954afe2c894095307f3f3f4c65bb5))
* **tree:** add the addon tree page with batching, undo and overrides ([dba077b](https://github.com/brunofgmag/fs-organizer/commit/dba077b7fd4629da365bb6bc370f3fc0976bb7d2))
* **tree:** derive the addon tree state from the enabled index ([e560080](https://github.com/brunofgmag/fs-organizer/commit/e560080ba33bb00dadfe7fe346d08ac1e354398d))
* **tree:** mark the addon linked away from the destination the profile picked ([dc3823a](https://github.com/brunofgmag/fs-organizer/commit/dc3823ae6cf292d3593c10d079284d7a3548b31e))
* **tree:** search the addon tree and hide empty categories ([ce4efb6](https://github.com/brunofgmag/fs-organizer/commit/ce4efb6f3d2e2cdb113a44d7a1be8b02341202cd))
* **view:** count the addon the simulator will load twice, and offer to see it ([6fde5fe](https://github.com/brunofgmag/fs-organizer/commit/6fde5fec3cda49a28ff9c82aaf0a0a084509d6d5))
* **view:** create, edit and apply presets from a screen of their own ([5df434f](https://github.com/brunofgmag/fs-organizer/commit/5df434f823285fa4de0e441694e90755ae4d4764))
* **view:** dress the app in the Modernist theme and embark the brand ([cc016d4](https://github.com/brunofgmag/fs-organizer/commit/cc016d4fe48338372362c8aeffac18744bc567d7))
* **view:** dress the six dialogs and the wizard in the theme ([ea33b19](https://github.com/brunofgmag/fs-organizer/commit/ea33b19b8b0be67e984f7c3e5b547d9d13901f3e))
* **view:** edit and remove a profile from the options screen ([8628d8a](https://github.com/brunofgmag/fs-organizer/commit/8628d8ab456ce2a1ffb7f83e2666c5190a2b34fa))
* **view:** give the header gear the screen it never had ([45254ff](https://github.com/brunofgmag/fs-organizer/commit/45254ff59e75a5d4378bc35a1b55e92c4a6f233d))
* **view:** give the journal the hover and the tooltip the other four screens have ([e69b885](https://github.com/brunofgmag/fs-organizer/commit/e69b885c00297864e40992235d1654e23e41dd4c))
* **view:** give the preset list a header, its content and its date ([d7e8dc4](https://github.com/brunofgmag/fs-organizer/commit/d7e8dc402865a9bd403f7a1f9eec6681a6993a73))
* **view:** give the screens a context panel that the selection summons ([02e3f45](https://github.com/brunofgmag/fs-organizer/commit/02e3f4515f9a24ed039faf1305da9ddeb00befea))
* **view:** let the folded panel spell out what is selected instead of its own label ([9f65345](https://github.com/brunofgmag/fs-organizer/commit/9f65345fa2796d940c6bc199b3dc2ae44360ec8b))
* **view:** let the presets toolbar filter the list the mockup always drew ([089685a](https://github.com/brunofgmag/fs-organizer/commit/089685a7f54d1b5190453d2a32e45ca2a4081104))
* **view:** let the two screens that know a defect tell the rail to raise its mark ([3765ad1](https://github.com/brunofgmag/fs-organizer/commit/3765ad106cc9ce35c4e7c4948b7f4799258e5eaf))
* **viewmodel:** count what needs attention among the Community entries ([871e173](https://github.com/brunofgmag/fs-organizer/commit/871e1737086accb265abb2295801e1fd630d2844))
* **viewmodel:** give the library tree its columns and the tags a row wears ([a4f5bfd](https://github.com/brunofgmag/fs-organizer/commit/a4f5bfd48994336b60ec90eb9934abd710cc9a16))
* **viewmodel:** give the preset screen the names it refuses and the counts it shows ([a09e51c](https://github.com/brunofgmag/fs-organizer/commit/a09e51ccfb62ac0551cce944b9c3e4cf5f93d8b9))
* **viewmodel:** give the tree view model the rules the screen needs ([0dbda3b](https://github.com/brunofgmag/fs-organizer/commit/0dbda3bfcc0c33c7f0b80cbe5fe0e9c86b359d9a))
* **viewmodel:** tell the options screen what it can show and change ([4a70c80](https://github.com/brunofgmag/fs-organizer/commit/4a70c80bc9815a187b11fce501aceda682469544))
* **view:** offer the whole text when the column cut it short ([e90854b](https://github.com/brunofgmag/fs-organizer/commit/e90854b86778227d35c8f50a2e598df3e27fae6b))
* **view:** organise the library and repair strays from the tree context menu ([8ca720a](https://github.com/brunofgmag/fs-organizer/commit/8ca720aa4ac1f79c2d87429cfa11aa2359d4bfe6))
* **view:** paint the pill, the tone and the alarm of a row from one delegate ([0e37d46](https://github.com/brunofgmag/fs-organizer/commit/0e37d46680a9624261513f2ff691ae28e9b31345))
* **view:** rebuild the Community screen and describe the batch it holds ([332efe7](https://github.com/brunofgmag/fs-organizer/commit/332efe73b024abfae40e3a3bd9469f72c749bab5))
* **view:** rebuild the journal screen on the theme and the panel ([43e0f0c](https://github.com/brunofgmag/fs-organizer/commit/43e0f0c0adca56f5f2e740a30d1576be9e6085cc))
* **view:** rebuild the library screen on the theme, the panel and the delegate ([98565cd](https://github.com/brunofgmag/fs-organizer/commit/98565cd133d47c4123079353d869acf4beee3391))
* **view:** rebuild the presets screen and centre the checkbox in its column ([002aa5c](https://github.com/brunofgmag/fs-organizer/commit/002aa5c4c7e89be0c9c58dfdcc48025ae77a0698))
* **view:** rebuild the quarantine screen and confirm what a restore touches ([d987af1](https://github.com/brunofgmag/fs-organizer/commit/d987af1649fba78e608c96a0a27ca8b134cfcb49))
* **view:** show the migration before it happens ([44b268c](https://github.com/brunofgmag/fs-organizer/commit/44b268c8c3e23bff0fa2466912faffca7b09dcb0))
* **view:** trade the sidebar for tabs that carry counts and a triage strip ([35799fb](https://github.com/brunofgmag/fs-organizer/commit/35799fb938cbd4c9d7a251162acb4d249c77c930))


### Bug Fixes

* **app:** make the session own the simulator warning and the settings it cannot read ([803db07](https://github.com/brunofgmag/fs-organizer/commit/803db0766d95bd7d80f3e51a4b00ad2bcd601c25))
* **domain:** count a category, not every child of the library ([acf673f](https://github.com/brunofgmag/fs-organizer/commit/acf673f307ebb6ac6bdf84286ec650b36078bb60))
* **domain:** give the same comparison key to paths that name the same folder ([8fba110](https://github.com/brunofgmag/fs-organizer/commit/8fba110e712ec8b5b1751b2031a8429e9986d7c8))
* **fileops:** create the way to the destination before moving ([c2cd517](https://github.com/brunofgmag/fs-organizer/commit/c2cd517dcf79f78dbd6d40f725ef8ba4ee0ebeb7))
* **fileops:** stop the probes from reporting an empty folder they never read ([9e1f50b](https://github.com/brunofgmag/fs-organizer/commit/9e1f50bbbeb836b8ef3893f00300954aa2c24662))
* **journal:** give the log a value for an outcome it cannot vouch for ([25c420b](https://github.com/brunofgmag/fs-organizer/commit/25c420bde2d98ed233da3c8ea7a52f318c5a3dd5))
* **journal:** stop the journal from lying about results and about which side is the source ([1550673](https://github.com/brunofgmag/fs-organizer/commit/1550673067cb6f58a4482ee1d667dde286cf4a04))
* make the Linux job green, and the domain separator-agnostic ([#1](https://github.com/brunofgmag/fs-organizer/issues/1)) ([420d1c5](https://github.com/brunofgmag/fs-organizer/commit/420d1c58e039625b10fca6a45fdfd62e9ca3d67c))
* **preset:** refuse a name that would write outside the presets folder ([6152da2](https://github.com/brunofgmag/fs-organizer/commit/6152da2a6c2340c4eee6cc61f3a9a9206b380875))
* **view:** carry the open categories along when one of them is renamed ([5bc3bee](https://github.com/brunofgmag/fs-organizer/commit/5bc3beefe16edd6df4286fd4144ac1415b7d2622))
* **view:** give the footer the gutter every other row of the page has ([af31c4a](https://github.com/brunofgmag/fs-organizer/commit/af31c4a8322277fdb09af755b4feaba810fb7526))
* **view:** let a dragged column take its room from the one after it ([7683be8](https://github.com/brunofgmag/fs-organizer/commit/7683be8cdcb7efc90a529f5cf06801ceb3078f85))
* **view:** let the tab you came from stay marked after leaving the options ([4685c18](https://github.com/brunofgmag/fs-organizer/commit/4685c1879e49a65eeec9bc21ca14cf613d1f800f))
* **view:** make the row, not the cell, the unit the delegate paints ([bf03b97](https://github.com/brunofgmag/fs-organizer/commit/bf03b97fb7ec69e864c8ed6e4c9292155cd06631))
* **view:** measure the columns when the content arrives and let a chosen column take the slack ([92896be](https://github.com/brunofgmag/fs-organizer/commit/92896bec5ec503a7230b103072717ac098b23db6))
* **viewmodel:** ask before one click would flip more than ten addons ([34ebc11](https://github.com/brunofgmag/fs-organizer/commit/34ebc11659a7a85e0bbafa41b8086600eff9e78b))
* **viewmodel:** let a duplicated entry look like the defect the spec says it is ([b26fb05](https://github.com/brunofgmag/fs-organizer/commit/b26fb0592ea9ccbb46f7d4ff0d3ee88ec058bfee))
* **viewmodel:** name a move target by its path in the library so two of the same name stay apart ([bc58f27](https://github.com/brunofgmag/fs-organizer/commit/bc58f27a27e01172ac19bdc9b06113752f52e86d))
* **viewmodel:** stop repeating the cell text back as its own tooltip ([3b71ef3](https://github.com/brunofgmag/fs-organizer/commit/3b71ef35ec185501259f8fe8b1a2b11302b83d08))
* **view:** punctuate the interface the way the rest of it is punctuated ([3bd58bf](https://github.com/brunofgmag/fs-organizer/commit/3bd58bf6f858ba988ad1fd1c9c231c6a5f8a2508))
* **view:** put the rail spine back on the axis the dot sits on ([c6ce81f](https://github.com/brunofgmag/fs-organizer/commit/c6ce81f736745ea4e9696b53bd46d2d706967254))
* **view:** stop a stray scroll from changing the value under the pointer ([cf1b46e](https://github.com/brunofgmag/fs-organizer/commit/cf1b46e3e7500e4d3339535c9f194e9c1456496a))
* **view:** stop a stray wheel from changing an action nobody clicked ([624f871](https://github.com/brunofgmag/fs-organizer/commit/624f87111242a317787f145cfec1f6a3bd2abbcc))
* **view:** stop offering a leftover folder as a place to import into ([34743a5](https://github.com/brunofgmag/fs-organizer/commit/34743a520619a7e5102b746c65c3398f37a45733))
* **view:** write every path the way windows writes it, and say when the journal knows nothing ([92cba92](https://github.com/brunofgmag/fs-organizer/commit/92cba9276a560a7aa0f55e800b30f3741ef05610))


### Performance Improvements

* **viewmodel:** measure the size of an import without blocking the screen ([6c6dd26](https://github.com/brunofgmag/fs-organizer/commit/6c6dd262310c9829ec68799ea44d5986a5680fba))
* **view:** stop measuring an advance no suffix or tag will use ([863ff8d](https://github.com/brunofgmag/fs-organizer/commit/863ff8d74a61e9d4bb51e501ec3685df800c331f))
* **view:** stop the row delegate from re-measuring and repainting what did not change ([2561576](https://github.com/brunofgmag/fs-organizer/commit/25615763d6123f18e2d82a6f9b1bf375f7fb1c55))
