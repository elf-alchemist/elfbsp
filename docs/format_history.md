# History of extended formats

* `1998-05-03` — Lee Killough defines the first attempt at an internal blockmap builder during Boom's development [^boom-blockmap-part1]
* `1998-10-06` — James Flynn disables Killough's first attempt, in favor a new and improved algorithm [^boom-blockmap-part2]
* `1998-10` —  Lee Killough creates yet another blockmap rebuilding algorithm in MBF [^mbf-blockmap]
* `1999-09` — Andrew Apted starts working on glBSP and designing the earliest spec for GL nodes
* `2000-06` — Andrew Apted releases EDGE, with support for GL nodes, rebuilding with internal builder if not present
* `2000-09-04` — Earliest SVN revision of glBSP, already contains V1 and V2 GL nodes [^glbsp-v1-v2]  
???
* `2003-09-24` — (from Randi's devlog) adds support for ZNOD on ZDoom
* `2003-09-27` — (from Randi's devlog) adds support for ZGLN on ZDoom??  
???
* `2004-04-30` — Deep (Jack) discusses on Doomworld the creation and design of a new node format [^deepbspv4-origin]
* `2004-07-13` — glBSP adds GL V3 nodes [^glbsp-glv3]
* `2004-10-09` — DeePsea 11.92f (quietly) releases with DeePBSPV4 and GL V4 nodes support [^deepbspv4-release]
* `2004-11-04` — Colin Phipps creates the original PrBoom reject padding implementation, without overflow emulation [^prboom-reject-padding]
* `2004-11-24` — Graf Zahl  points out concerns over the limitations of the GL V3 spec, prompting the sharing of the GL V4 spec, and the later development of GL V5 [^glv3-concern]
* `2005-04-19` — Andrew Apted starts a discussion on the development of GL V5 nodes [^glv5-origin]
* `2005-05-20` — Andrew Apted adds GL V5 nodes to glBSP [^glbsp-glv5]
* `2005-05-20` — Andrew Apted adds ZNOD support to glBSP [^glbsp-znod]
* `2005-07-09` — GL V5 spec is publicly frozen [^glv5-spec]
* `2006-02-24` — earliest SVN revision of ZDoom, already supports ZNOD and ZGLN nodes
* `2006-07-21` — Andrey Budko adds in overflow emulation for reject padding [^prboom-reject-overflow]
* `2006-10-07` — Andrey Budko adds CLI flag `-reject_pad_with_ff` for erroneous historical PrBoom padding [^prboom-reject-ff]
* `2006-11-05` — Kaiser publishes the "Console Doom hacking project" thread, with the first description of the LEAFS lump [^console-leafs]
* `2009-03-17` — Randi adds ZGL2 (with 32bit sidedef indexes to address large UDMF maps) to ZDoom/ZDBSP [^zdoom-zgl2]
* `2010-04-12` — Sunder's development unearths the DeePBSPV4 format and triggers a discussion for its wider adoption [^sunder-deepbspv4] as well as the issues of the compressed ZDoom nodes
* `2010-04-15` — Andrey Budko adds DeePBSPV4 node support to PrBoom+ [^prboom-deepbspv4]
* `2010-04-17` — Graf Zahl adds DeePBSPV4 node support to ZDoom [^zdoom-deepbspv4]
* `2010-04-17` — Graf Zahl adds the uncompressed versions, XNOD, XGLN and XGL2, to ZDoom/ZDBSP [^zdoom-xnod-xgln]
* `2010-04-18` — Andrey Budko adds support for XNOD [^prboom-xnod]
* `2010-04-23` — Fraggle adds PrBoom+'s reject matrix overflow emulation padding to Chocolate-Doom [^chocolate-reject-part1]
* `2012-11-08` — Andrew Apted starts importing glBSP [^eureka-glbsp-part1]
* `2012-12-07` — Randi adds ZGL3 and XGL3 to ZDoom/ZDBSP, addressing coordinate precision on BSP nodes [^zdoom-xgl3-zgl3]
* `2013-04-25` — Andrew Apted completes the initial Eureka+glBSP integration [^eureka-glbsp-part2]
* `2015-02-10` — Fabian Greffrath adds support for DeePBSPV4, XNOD and ZNOD formats to Crispy-Doom [^crispy-doom-extnodes]
* `2016-11-30` — Andrew Apted adds XNOD support to Eureka [^eureka-xnod]
* `2019-06-14` — Fabian Greffrath and Graf Zahl retroactively add ZNOD support to PrBoom+um [^prboom-znod]
* `2019-10-20` — Andrew Apted adds XGL3 support to Eureka [^eureka-xgl3]
* `2020-01-10` — Fabian Greffrath adds support for reject matrix overflow emulation padding in Woof! [^woof-reject]
* `2020-02-28` — Fabian Greffrath adds support for DeePBSPV4, XNOD and ZNOD formats in Woof! [^woof-extnodes]
* `2021-02-17` — Roman Fomin restores James Flynn's Boom blockmap algorithm for the purposes of demo compatibility with PrBoom+ [^woof-boom-blockmap]
* `2023-01-14` — Kraflab adds the reading of XGLN/ZGLN format in UDMF maps, without adding XGLN segs [^dsda-xgl-part1]
* `2023-01-23` — Kraflab implements code for parsing XGLN/ZGLN Segs in DSDA [^dsda-xgl-part2]
* `2023-01-23` — Roman Fomin adds DeePBSPV4, XNOD and ZNOD formats to Crispy-Heretic [^crispy-heretic-extnodes]
* `2023-01-30` — Kraflab adds XGL2/ZGL2 and XGL3/ZGL3 support to DSDA, for UDMF maps [^dsda-xgl-part3] and adds XGLN/2/3 node support to binary format maps [^dsda-xgl-part4]
* `2023-08-24` — Fabian Greffrath adds support for XGLN and ZGLN in Woof! [^woof-xgl-part1]
* `2025-02-28` — Diema forks Kaiser's D64BSP, starting and adding a few build-time special effects over the next month or so [^d64bsp-diema]
* `2025-09-10` — Noseey ports Chocolate's reject matrix overflow emulation padding function to work on all of Chocolate Doom, Heretic, Hexen & Strife [^chocolate-reject-part2]
* `2025-11-25` — Elf-Alchemist adds XGL2/ZGL2 & XGL3/ZGL3 format support to Woof! [^woof-xgl-part2]
* `2025-12-08` — Immorpher forks Diema's DMA-BSP64, starting BSP64-Enhanced and adding even more special effects and improving sidedef compression [^d64bsp-enhanced]
* `2026-04-01` — Elf-Alchemist adds all the remaining node formats, and extended vanilla blockmap support, to Crispy Doom, Heretic, Hexen and Strife [^crispy-full-extnodes-blockmap]
* `2026-04-14` — Elf-Alchemist adds XBM1 blockmap lump support to ELFBSP [^elfbsp-xbm1]
* `2026-04-14` — Elf-Alchemist adds Doom64 LEAFS format support to ELFBSP, with vanilla and DeePBSPV4 lump support [^elfbsp-leafs]
* `2026-04-16` — Elf-Alchemist adds XBM1 blockmap lump support to Woof! [^woof-xbm1]

[^boom-blockmap-part1]: https://github.com/doom-cross-port-collab/boom/blob/2a11d37040be9ac40a61f4c7b7f4541bef2411bf/src/p_setup.c#L921-L923
[^boom-blockmap-part2]: https://github.com/doom-cross-port-collab/boom/blob/2a11d37040be9ac40a61f4c7b7f4541bef2411bf/src/p_setup.c#L605-L609
[^mbf-blockmap]: https://github.com/doom-cross-port-collab/mbf/blob/8f705abd36ed2edc3ba64abb855f05a12dd71bc9/src/p_setup.c#L535-L546
[^glbsp-v1-v2]: https://github.com/elf-alchemist/glbsp-archive/commit/12188e7b46bb27422b452f972315631c308bb7bc
[^deepbspv4-origin]: https://www.doomworld.com/forum/topic/23391-new-node-format/
[^glbsp-glv3]: https://github.com/elf-alchemist/glbsp-archive/commit/efdeccfcfd56016ce2e18ec8dc90e4eb2c668eb9
[^deepbspv4-release]: https://web.archive.org/web/20041130085424/http://www.sbsoftware.com/
[^prboom-reject-padding]: https://github.com/kraflab/dsda-doom/commit/b74891072f9c84db0e7bea243ca7ca886e59eb96
[^glv3-concern]: https://www.doomworld.com/forum/topic/29546-important-glnodes-v3-question/
[^glv5-origin]: https://www.doomworld.com/forum/topic/31695-new-gl-nodes-spec-v5/
[^glbsp-glv5]: https://github.com/elf-alchemist/glbsp-archive/commit/182c158da4aa918eccd2d25b42c7f3d4c2e5eec8
[^glbsp-znod]: https://github.com/elf-alchemist/glbsp-archive/commit/40fb3f670ea99920c51f5b06b1fdbf582e7af24a
[^glv5-spec]: https://www.doomworld.com/forum/topic/32807-v5-gl-node-update/
[^prboom-reject-overflow]: https://github.com/kraflab/dsda-doom/commit/b9866ccdcdf152b88c1ae0b46bd6a584af84b59c
[^prboom-reject-ff]: https://github.com/kraflab/dsda-doom/commit/a119dc6eb14a602f6c2a092c2bcc0d9038bb400f
[^console-leafs]: https://www.doomworld.com/forum/topic/38608-the-console-doom-hacking-project-console-specs/
[^zdoom-zgl2]: https://github.com/UZDoom/UZDoom/commit/314216343dae34a70993803375dfce8ebe7090fb
[^sunder-deepbspv4]: https://www.doomworld.com/forum/post/867492
[^prboom-deepbspv4]: https://github.com/kraflab/dsda-doom/commit/42972ed42d3a54bc9ac0636e17f3db1b2ee7aaa9
[^zdoom-deepbspv4]: https://github.com/UZDoom/UZDoom/commit/da99577cbf7158a42591c288c2d6f60948929fbe
[^zdoom-xnod-xgln]: https://github.com/UZDoom/UZDoom/commit/c85a602546c59f5bfc939d609445f51a9b22036e
[^prboom-xnod]: https://github.com/kraflab/dsda-doom/commit/8b481ebeb73a7f867e778a0ecffe474bc2d0784d
[^chocolate-reject-part1]: https://github.com/chocolate-doom/chocolate-doom/commit/4070ecd92c45dadc80e048e1bc929bced925c232
[^eureka-glbsp-part1]: https://github.com/ioan-chera/eureka-editor/commit/816fcdfe777a49f8b20bd3f162904a7b3f2baf58
[^zdoom-xgl3-zgl3]: https://github.com/UZDoom/UZDoom/commit/d77297e969f138ed1249589b15f1833bce2bdc77
[^crispy-doom-extnodes]: https://github.com/fabiangreffrath/crispy-doom/commit/9c108dc2b637fc4f3e7c1401637de54f075e6fd4
[^eureka-glbsp-part2]:https://github.com/ioan-chera/eureka-editor/commit/7fae899461d29b812c8648de9adceb6638f921b4
[^eureka-xnod]: https://github.com/ioan-chera/eureka-editor/commit/426358794acd32d82efddc0542255b149b67db2b
[^prboom-znod]: https://github.com/kraflab/dsda-doom/commit/aaa3877f83277a12bbcd8b14e8c204ed048066d2
[^eureka-xgl3]: https://github.com/ioan-chera/eureka-editor/commit/de449b796f02c30b04473c15884e79d58b70a4e7
[^woof-reject]: https://github.com/fabiangreffrath/woof/commit/00e17c0e1fe95a922957ba5e458c2ab514cd3650
[^woof-extnodes]: https://github.com/fabiangreffrath/woof/commit/0a690dc10e0c985b3e3a29757d5ac0e8635e0e8b
[^woof-boom-blockmap]: https://github.com/fabiangreffrath/woof/commit/b1daa874c56640beebcdb4649dce2f51eb5c7174
[^dsda-xgl-part1]: https://github.com/kraflab/dsda-doom/commit/9caaa25824765338f631027d2ecd79cb8ee57a9d
[^dsda-xgl-part2]: https://github.com/kraflab/dsda-doom/commit/80a9b536be7437bdc31bdc0b283301f46f0108bb
[^crispy-heretic-extnodes]: https://github.com/fabiangreffrath/crispy-doom/commit/3a62c5364f8dfd73e958833f155e583a374ba472
[^dsda-xgl-part3]: https://github.com/kraflab/dsda-doom/commit/c79da61db00a97d69178090b42adf7fa309f1511
[^dsda-xgl-part4]: https://github.com/kraflab/dsda-doom/commit/20ff581810aabf6d3ec4825b5886a483df5d41e2
[^woof-xgl-part1]:https://github.com/fabiangreffrath/woof/commit/483e4e7892a1b3e0d0f5fec592bee71a66da1b65
[^d64bsp-diema]: https://github.com/darkhaven3/dma-bsp64/commits/490fea6b330fc1e23c4015af311c2f87ea17bcd8
[^chocolate-reject-part2]: https://github.com/chocolate-doom/chocolate-doom/commit/c52a135e5a606fa1fc91e68845d3eae6ac6adefc
[^woof-xgl-part2]: https://github.com/fabiangreffrath/woof/commit/cd9f22574039d3473c37e7f02072f338f0273d30
[^d64bsp-enhanced]: https://github.com/darkhaven3/dma-bsp64/compare/490fea6b...Immorpher:BSP64Enhanced:66482413
[^crispy-full-extnodes-blockmap]: https://github.com/fabiangreffrath/crispy-doom/commit/27ca593c30ae56528e388733b1eb5dc4fef43349
[^elfbsp-xbm1]: https://github.com/elf-alchemist/elfbsp/commit/f06fa8f564ac61afcea7c5e8ca3b03c873cbe02c
[^elfbsp-leafs]: https://github.com/elf-alchemist/elfbsp/commit/26116023a3bd5682a42039fa3e1ad6ddc5de9ed1
[^woof-xbm1]: https://github.com/fabiangreffrath/woof/commit/d99cf96cb3d3f21c4e9fbaf2c70f15477aabde25
