# History of extended formats

* `1999-09` — Andrew Apted starts working on glBSP and designing the earliest spec for GL nodes
* `2000-06` — releases EDGE, with support for GL nodes, rebuilding with internal builder if not present  
???
* `2003-09-24` — (from Randi's devlog) adds support for ZNOD on ZDoom
* `2003-09-27` — (from Randi's devlog) adds support for ZGLN on ZDoom??  
???
* `2004-04-30` — Deep (Jack)  discusses on Doomworld the creation and design of a new node format [1]
* `2004-10-09` — DeePsea 11.92f (quietly) releases with DeePBSPV4 and GL V4 node support [2]
* `2006-02-24` — earliest SVN revision of ZDoom, already supports ZNOD and ZGLN nodes
* `2009-03-17` — Randi adds ZGL2 (with 32bit sidedef indexes to address large UDMF maps) to ZDoom/ZDBSP [3]
* `2010-04-12` — Sunder's development unearths the DeePBSPV4 format and triggers a discussion for its wider adoption [4]
* `2010-04-15` — Andrey Budko adds DeepBSPV4 node support to PrBoom+ [5]
* `2010-04-17` —  Graf adds DeepBSPV4 node support to ZDoom [6]
* `2010-04-17` — Graf adds the uncompressed versions, XNOD, XGLN and XGL2, to ZDoom/ZDBSP [7]
* `2010-04-18` — Andrey Budko adds support for XNOD [8]
* `2012-12-07` — Randi adds ZGL3 and XGL3 to ZDoom/ZDBSP, addressing coordinate precision on BSP nodes [9]
* `2019-06-14` — Fabian and Graf retroactively add XNOD support to PrBoom+um [10]

[1]: <https://www.doomworld.com/forum/topic/23391-new-node-format/>
[2]: <https://web.archive.org/web/20041130085424/http://www.sbsoftware.com/>
[3]: <https://github.com/UZDoom/UZDoom/commit/314216343dae34a70993803375dfce8ebe7090fb>
[4]: <https://www.doomworld.com/forum/post/867492>
[5]: <https://github.com/kraflab/dsda-doom/commit/42972ed42d3a54bc9ac0636e17f3db1b2ee7aaa9>
[6]: <https://github.com/UZDoom/UZDoom/commit/da99577cbf7158a42591c288c2d6f60948929fbe>
[7]: <https://github.com/UZDoom/UZDoom/commit/c85a602546c59f5bfc939d609445f51a9b22036e>
[8]: <https://github.com/kraflab/dsda-doom/commit/8b481ebeb73a7f867e778a0ecffe474bc2d0784d>
[9]: <https://github.com/UZDoom/UZDoom/commit/d77297e969f138ed1249589b15f1833bce2bdc77>
[10]: <https://github.com/kraflab/dsda-doom/commit/aaa3877f83277a12bbcd8b14e8c204ed048066d2>
