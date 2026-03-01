# History of extended formats

* `1999-09` — Andrew Apted starts working on glBSP and designing the earliest spec for GL nodes
* `2000-06` — releases EDGE, with support for GL nodes, rebuilding with internal builder if not present  
???
* `2003-09-24` — (from Randi's devlog) adds support for ZNOD on ZDoom
* `2003-09-27` — (from Randi's devlog) adds support for ZGLN on ZDoom??  
???
* `2004-04-30` — Deep (Jack) discusses on Doomworld the creation and design of a new node format [^https://www.doomworld.com/forum/topic/23391-new-node-format/]
* `2004-10-09` — DeePsea 11.92f (quietly) releases with DeePBSPV4 and GL V4 node support [^2]
* `2006-02-24` — earliest SVN revision of ZDoom, already supports ZNOD and ZGLN nodes
* `2009-03-17` — Randi adds ZGL2 (with 32bit sidedef indexes to address large UDMF maps) to ZDoom/ZDBSP [^3]
* `2010-04-12` — Sunder's development unearths the DeePBSPV4 format and triggers a discussion for its wider adoption [^4] as well as the issues of the compressed ZDoom nodes
* `2010-04-15` — Andrey Budko adds DeepBSPV4 node support to PrBoom+ [^5]
* `2010-04-17` — Graf adds DeepBSPV4 node support to ZDoom [^6]
* `2010-04-17` — Graf adds the uncompressed versions, XNOD, XGLN and XGL2, to ZDoom/ZDBSP [^7]
* `2010-04-18` — Andrey Budko adds support for XNOD [^8]
* `2012-12-07` — Randi adds ZGL3 and XGL3 to ZDoom/ZDBSP, addressing coordinate precision on BSP nodes [^9]
* `2019-06-14` — Fabian and Graf retroactively add XNOD support to PrBoom+um [^10]
* `2023-01-14` — Kraflab adds the reading of XGLN/ZGLN format in UDMF maps, without adding XGLN segs [^11]
* `2023-01-23` — Kraflab implements code for parsing XGLN/ZGLN Segs support in DSDA [^12]
* `2023-01-30` — Kraflab adds XGL2/ZGL2 and XGL3/ZGL3 support to DSDA, for UDMF maps [^13] and adds XGLN/2/3 node support to binary format maps [^14]

[^2]: https://web.archive.org/web/20041130085424/http://www.sbsoftware.com/
[^3]: https://github.com/UZDoom/UZDoom/commit/314216343dae34a70993803375dfce8ebe7090fb
[^4]: https://www.doomworld.com/forum/post/867492
[^5]: https://github.com/kraflab/dsda-doom/commit/42972ed42d3a54bc9ac0636e17f3db1b2ee7aaa9
[^6]: https://github.com/UZDoom/UZDoom/commit/da99577cbf7158a42591c288c2d6f60948929fbe
[^7]: https://github.com/UZDoom/UZDoom/commit/c85a602546c59f5bfc939d609445f51a9b22036e
[^8]: https://github.com/kraflab/dsda-doom/commit/8b481ebeb73a7f867e778a0ecffe474bc2d0784d
[^9]: https://github.com/UZDoom/UZDoom/commit/d77297e969f138ed1249589b15f1833bce2bdc77
[^10]: https://github.com/kraflab/dsda-doom/commit/aaa3877f83277a12bbcd8b14e8c204ed048066d2
[^11]: https://github.com/kraflab/dsda-doom/commit/9caaa25824765338f631027d2ecd79cb8ee57a9d
[^12]: https://github.com/kraflab/dsda-doom/commit/80a9b536be7437bdc31bdc0b283301f46f0108bb
[^13]: https://github.com/kraflab/dsda-doom/commit/c79da61db00a97d69178090b42adf7fa309f1511
[^14]: https://github.com/kraflab/dsda-doom/commit/20ff581810aabf6d3ec4825b5886a483df5d41e2
