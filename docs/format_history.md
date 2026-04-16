# History of extended formats

* `1999-09` — Andrew Apted starts working on glBSP and designing the earliest spec for GL nodes
* `2000-06` — releases EDGE, with support for GL nodes, rebuilding with internal builder if not present
* `2000-09-04` — Earliest SVN revision of glBSP, already contains V1 and V2 GL nodes [^glbsp-v1-v2]  
???
* `2003-09-24` — (from Randi's devlog) adds support for ZNOD on ZDoom
* `2003-09-27` — (from Randi's devlog) adds support for ZGLN on ZDoom??  
???
* `2004-04-30` — Deep (Jack) discusses on Doomworld the creation and design of a new node format [^deepbspv4-origin]
* `2004-07-13` — glBSP adds GL V3 nodes [^glbsp-glv3]
* `2004-10-09` — DeePsea 11.92f (quietly) releases with DeePBSPV4 and GL V4 nodes support [^deepbspv4-release]
* `2004-11-24` — Graf Zahl  points out concerns over the limitations of the GL V3 spec, prompting the sharing of the GL V4 spec, and the later development of GL V5 [^glv3-concern]
* `2005-04-19` — Andrew Apted starts a discussion on the development of GL V5 nodes [^glv5-origin]
* `2005-05-20` — glBSP adds GL V5 nodes [^glbsp-glv5]
* `2005-05-20` — glBSP adds ZNOD support [^glbsp-znod]
* `2005-07-09` — GL V5 spec is publicly frozen [^glv5-spec]
* `2006-02-24` — earliest SVN revision of ZDoom, already supports ZNOD and ZGLN nodes
* `2006-11-05` — Kaiser publishes the "Console Doom hacking project" thread, with the first description of the LEAFS lump [^console-leafs]
* `2009-03-17` — Randi adds ZGL2 (with 32bit sidedef indexes to address large UDMF maps) to ZDoom/ZDBSP [^zdoom-zgl2]
* `2010-04-12` — Sunder's development unearths the DeePBSPV4 format and triggers a discussion for its wider adoption [^sunder-deepbspv4] as well as the issues of the compressed ZDoom nodes
* `2010-04-15` — Andrey Budko adds DeePBSPV4 node support to PrBoom+ [^prboom-deepbspv4]
* `2010-04-17` — Graf adds DeePBSPV4 node support to ZDoom [^zdoom-deepbspv4]
* `2010-04-17` — Graf adds the uncompressed versions, XNOD, XGLN and XGL2, to ZDoom/ZDBSP [^zdoom-xnod-xgln]
* `2010-04-18` — Andrey Budko adds support for XNOD [^prboom-xnod]
* `2012-12-07` — Randi adds ZGL3 and XGL3 to ZDoom/ZDBSP, addressing coordinate precision on BSP nodes [^zdoom-xgl3-zgl3]
* `2015-02-10` — Fabian adds support for DeePBSPV4, XNOD and ZNOD formats to Crispy-Doom [^crispy-doom-extnodes]
* `2016-11-30` — Apted adds XNOD support to Eureka [^eureka-xnod]
* `2019-06-14` — Fabian and Graf retroactively add ZNOD support to PrBoom+um [^prboom-znod]
* `2019-10-20` — Apted adds XGL3 support to Eureka [^eureka-xgl3]
* `2023-01-14` — Kraflab adds the reading of XGLN/ZGLN format in UDMF maps, without adding XGLN segs [^dsda-xgl-part1]
* `2023-01-23` — Kraflab implements code for parsing XGLN/ZGLN Segs support in DSDA [^dsda-xgl-part2]
* `2023-01-23` — Roman Fomin adds DeePBSPV4, XNOD and ZNOD formats to Crispy-Heretic [^crispy-heretic-extnodes]
* `2023-01-30` — Kraflab adds XGL2/ZGL2 and XGL3/ZGL3 support to DSDA, for UDMF maps [^dsda-xgl-part3] and adds XGLN/2/3 node support to binary format maps [^dsda-xgl-part4]
* `2026-04-01` — Elf-Alchemist adds the remaining ZDBSP nodes to Crispy Doom, Heretic, Hexen and Strife [^crispy-zdbsp]

[^glbsp-v1-v2]: https://github.com/elf-alchemist/glbsp-archive/commit/12188e7b46bb27422b452f972315631c308bb7bc
[^deepbspv4-origin]: https://www.doomworld.com/forum/topic/23391-new-node-format/
[^glbsp-glv3]: https://github.com/elf-alchemist/glbsp-archive/commit/efdeccfcfd56016ce2e18ec8dc90e4eb2c668eb9
[^deepbspv4-release]: https://web.archive.org/web/20041130085424/http://www.sbsoftware.com/
[^glv3-concern]: https://www.doomworld.com/forum/topic/29546-important-glnodes-v3-question/
[^glv5-origin]: https://www.doomworld.com/forum/topic/31695-new-gl-nodes-spec-v5/
[^glbsp-glv5]: https://github.com/elf-alchemist/glbsp-archive/commit/182c158da4aa918eccd2d25b42c7f3d4c2e5eec8
[^glbsp-znod]: https://github.com/elf-alchemist/glbsp-archive/commit/40fb3f670ea99920c51f5b06b1fdbf582e7af24a
[^glv5-spec]: https://www.doomworld.com/forum/topic/32807-v5-gl-node-update/
[^console-leafs]: https://www.doomworld.com/forum/topic/38608-the-console-doom-hacking-project-console-specs/
[^zdoom-zgl2]: https://github.com/UZDoom/UZDoom/commit/314216343dae34a70993803375dfce8ebe7090fb
[^sunder-deepbspv4]: https://www.doomworld.com/forum/post/867492
[^prboom-deepbspv4]: https://github.com/kraflab/dsda-doom/commit/42972ed42d3a54bc9ac0636e17f3db1b2ee7aaa9
[^zdoom-deepbspv4]: https://github.com/UZDoom/UZDoom/commit/da99577cbf7158a42591c288c2d6f60948929fbe
[^zdoom-xnod-xgln]: https://github.com/UZDoom/UZDoom/commit/c85a602546c59f5bfc939d609445f51a9b22036e
[^prboom-xnod]: https://github.com/kraflab/dsda-doom/commit/8b481ebeb73a7f867e778a0ecffe474bc2d0784d
[^zdoom-xgl3-zgl3]: https://github.com/UZDoom/UZDoom/commit/d77297e969f138ed1249589b15f1833bce2bdc77
[^crispy-doom-extnodes]: https://github.com/fabiangreffrath/crispy-doom/commit/9c108dc2b637fc4f3e7c1401637de54f075e6fd4
[^eureka-xnod]: https://github.com/ioan-chera/eureka-editor/commit/426358794acd32d82efddc0542255b149b67db2b
[^prboom-znod]: https://github.com/kraflab/dsda-doom/commit/aaa3877f83277a12bbcd8b14e8c204ed048066d2
[^eureka-xgl3]: https://github.com/ioan-chera/eureka-editor/commit/de449b796f02c30b04473c15884e79d58b70a4e7
[^dsda-xgl-part1]: https://github.com/kraflab/dsda-doom/commit/9caaa25824765338f631027d2ecd79cb8ee57a9d
[^dsda-xgl-part2]: https://github.com/kraflab/dsda-doom/commit/80a9b536be7437bdc31bdc0b283301f46f0108bb
[^crispy-heretic-extnodes]: https://github.com/fabiangreffrath/crispy-doom/commit/3a62c5364f8dfd73e958833f155e583a374ba472
[^dsda-xgl-part3]: https://github.com/kraflab/dsda-doom/commit/c79da61db00a97d69178090b42adf7fa309f1511
[^dsda-xgl-part4]: https://github.com/kraflab/dsda-doom/commit/20ff581810aabf6d3ec4825b5886a483df5d41e2
[^crispy-zdbsp]: https://github.com/fabiangreffrath/crispy-doom/commit/27ca593c30ae56528e388733b1eb5dc4fef43349
