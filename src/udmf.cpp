
#include "core.hpp"
#include "local.hpp"
#include "scanner_support.h"
#include "wad.hpp"

#include "scanner.h"
#include <cctype>

//-----------------------------------------------------------------------------

enum class UNamespace
{
  Unknown,

  Doom,
  Heretic,
  Hexen,
  Strife,

  DelphiDoom,
  DelphiHeretic,
  DelphiHexen,
  DelphiStrife,
  DSDADoom,
  Eternity,
  EDGEClassic,
  Helion,
  Woof,
  ZDoom,

  KillingTime,
};

//-----------------------------------------------------------------------------

UNamespace udmf_namespace = UNamespace::Unknown;

inline static int32_t UScanInt(Scanner &s)
{
  int32_t x = 0;
  s.MustGetToken('=');

  bool neg = s.CheckToken('-');
  s.MustGetToken(TK_IntConst);
  x = neg ? -s->number : s->number;

  s.MustGetToken(';');
  return x;
}

inline static double UScanFloat(Scanner &s)
{
  double x = 0;
  s.MustGetToken('=');

  bool neg = s.CheckToken('-');
  s.MustGetToken(TK_FloatConst);
  x = neg ? -s->decimal : s->decimal;

  s.MustGetToken(';');
  return x;
}

inline static uint32_t UScanFlag(Scanner &s, uint32_t f)
{
  uint32_t x = 0;
  s.MustGetToken('=');

  s.MustGetToken(TK_BoolConst);
  if (s->boolean) x |= f;

  s.MustGetToken(';');
  return x;
}

inline static void UScanLumpName(Scanner &s, char *x)
{
  s.MustGetToken('=');
  s.MustGetToken(TK_StringConst);

  for (size_t index = 0; index < 8; index++)
  {
    x[index] = s->str[index];
    if (s->str[index] == '\0') break;
  }

  s.MustGetToken(';');
}

static inline void UScanSkip(Scanner &s)
{
  if (s.CheckToken('='))
  {
    while (s.TokensLeft())
    {
      if (s.CheckToken(';'))
      {
        break;
      }

      s.GetNextToken(true);
    }
    return;
  }

  s.MustGetToken('{');
  int brace_count = 1;
  while (s.TokensLeft())
  {
    if (s.CheckToken('}'))
    {
      --brace_count;
    }
    else if (s.CheckToken('{'))
    {
      ++brace_count;
    }
    if (!brace_count)
    {
      break;
    }
    s.GetNextToken(true);
  }
}

//-----------------------------------------------------------------------------

void ParseUNamespace(Scanner &s, Level &level)
{
  s.MustGetToken('=');
  s.MustGetToken(TK_StringConst);
  auto key = s->str;
  auto prop = key.data();
  CStringToLower(prop);

  if (!strcmp(prop, "doom"))
  {
    udmf_namespace = UNamespace::Doom;
  }
  else if (!strcmp(prop, "heretic"))
  {
    udmf_namespace = UNamespace::Heretic;
  }
  else if (!strcmp(prop, "hexen"))
  {
    udmf_namespace = UNamespace::Hexen;
  }
  else if (!strcmp(prop, "strife"))
  {
    udmf_namespace = UNamespace::Strife;
  }
  else if (!strcmp(prop, "delphidoom"))
  {
    udmf_namespace = UNamespace::DelphiDoom;
  }
  else if (!strcmp(prop, "delphiheretic"))
  {
    udmf_namespace = UNamespace::DelphiHeretic;
  }
  else if (!strcmp(prop, "delphihexen"))
  {
    udmf_namespace = UNamespace::DelphiHexen;
  }
  else if (!strcmp(prop, "delphistrife"))
  {
    udmf_namespace = UNamespace::DelphiStrife;
  }
  else if (!strcmp(prop, "dsda"))
  {
    udmf_namespace = UNamespace::DSDADoom;
  }
  else if (!strcmp(prop, "eternity"))
  {
    udmf_namespace = UNamespace::Eternity;
  }
  else if (!strcmp(prop, "edge-classic"))
  {
    udmf_namespace = UNamespace::EDGEClassic;
  }
  else if (!strcmp(prop, "helion"))
  {
    udmf_namespace = UNamespace::Helion;
  }
  else if (!strcmp(prop, "woof"))
  {
    udmf_namespace = UNamespace::Woof;
  }
  else if (!strcmp(prop, "zdoom"))
  {
    udmf_namespace = UNamespace::ZDoom;
  }
  else if (!strcmp(prop, "killingtime"))
  {
    udmf_namespace = UNamespace::KillingTime;
  }

  switch (udmf_namespace)
  {
  case UNamespace::Hexen:
  case UNamespace::DelphiHexen:
    level.po.anchor = Hexen_PolyObj_Anchor;
    level.po.spawn = Hexen_PolyObj_Spawn;
    level.po.spawn_crush = Hexen_PolyObj_SpawnCrush;
    level.po.spawn_hurt = Hexen_PolyObj_SpawnHurt;
    break;
  default:
    level.po.anchor = Hexen_PolyObj_Anchor;
    level.po.spawn = Hexen_PolyObj_Spawn;
    level.po.spawn_crush = Hexen_PolyObj_SpawnCrush;
    level.po.spawn_hurt = Hexen_PolyObj_SpawnHurt;
    break;
  }

  s.MustGetToken(';');
}

void ParseUVertex(Scanner &s, Level &level)
{
  UVertexMinimal vertex;
  UtilZero(&vertex);

  s.MustGetToken('{');
  while (!s.CheckToken('}'))
  {
    s.MustGetToken(TK_Identifier);
    auto key = s->str;
    auto prop = key.data();
    CStringToLower(prop);

    if (!strcmp(prop, "x"))
    {
      vertex.x = UScanFloat(s);
    }
    else if (!strcmp(prop, "y"))
    {
      vertex.y = UScanFloat(s);
    }
    else
    {
      UScanSkip(s);
    }
  }

  level.udmf_vertex.push_back(vertex);
}

void ParseULinedef(Scanner &s, Level &level)
{
  ULinedefMinimal linedef;
  UtilZero(&linedef);
  linedef.sideback = -1;

  s.MustGetToken('{');
  while (!s.CheckToken('}'))
  {
    s.MustGetToken(TK_Identifier);
    auto key = s->str;
    auto prop = key.data();
    CStringToLower(prop);

    if (!strcmp(prop, "v1"))
    {
      linedef.v1_id = static_cast<size_t>(UScanInt(s));
    }
    else if (!strcmp(prop, "v2"))
    {
      linedef.v2_id = static_cast<size_t>(UScanInt(s));
    }
    else if (!strcmp(prop, "special"))
    {
      linedef.special = UScanInt(s);
    }
    else if (!strcmp(prop, "id"))
    {
      linedef.id = UScanInt(s);
    }
    else if (!strcmp(prop, "arg0"))
    {
      linedef.args[0] = UScanInt(s);
    }
    else if (!strcmp(prop, "arg1"))
    {
      linedef.args[1] = UScanInt(s);
    }
    else if (!strcmp(prop, "arg2"))
    {
      linedef.args[2] = UScanInt(s);
    }
    else if (!strcmp(prop, "arg3"))
    {
      linedef.args[3] = UScanInt(s);
    }
    else if (!strcmp(prop, "arg4"))
    {
      linedef.args[4] = UScanInt(s);
    }
    else if (!strcmp(prop, "sidefront"))
    {
      linedef.sidefront = UScanInt(s);
    }
    else if (!strcmp(prop, "sideback"))
    {
      linedef.sideback = UScanInt(s);
    }
    else if (!strcmp(prop, "blocking"))
    {
      linedef.flags |= UScanFlag(s, ULF_Blocking);
    }
    else if (!strcmp(prop, "blockmonsters"))
    {
      linedef.flags |= UScanFlag(s, ULF_BlockMonsters);
    }
    else if (!strcmp(prop, "twosided"))
    {
      linedef.flags |= UScanFlag(s, ULF_TwoSided);
    }
    else if (!strcmp(prop, "dontpegtop"))
    {
      linedef.flags |= UScanFlag(s, ULF_DontPegTop);
    }
    else if (!strcmp(prop, "dontpegbottom"))
    {
      linedef.flags |= UScanFlag(s, ULF_DontPegBottom);
    }
    else if (!strcmp(prop, "secret"))
    {
      linedef.flags |= UScanFlag(s, ULF_Secret);
    }
    else if (!strcmp(prop, "blocksound"))
    {
      linedef.flags |= UScanFlag(s, ULF_BlockSound);
    }
    else if (!strcmp(prop, "dontdraw"))
    {
      linedef.flags |= UScanFlag(s, ULF_DontDraw);
    }
    else if (!strcmp(prop, "mapped"))
    {
      linedef.flags |= UScanFlag(s, ULF_Mapped);
    }
    else if (!strcmp(prop, "jumpover"))
    {
      linedef.flags |= UScanFlag(s, ULF_JumpOver);
    }
    else if (!strcmp(prop, "blockfloaters"))
    {
      linedef.flags |= UScanFlag(s, ULF_BlockFloaters);
    }
    else if (!strcmp(prop, "translucent"))
    {
      linedef.flags |= UScanFlag(s, ULF_Translucent);
    }
    else if (!strcmp(prop, "transparent"))
    {
      linedef.flags |= UScanFlag(s, ULF_Transparent);
    }
    else if (!strcmp(prop, "playercross"))
    {
      linedef.spac |= UScanFlag(s, ULS_PlayerCross);
    }
    else if (!strcmp(prop, "playeruse"))
    {
      linedef.spac |= UScanFlag(s, ULS_PlayerUse);
    }
    else if (!strcmp(prop, "monstercross"))
    {
      linedef.spac |= UScanFlag(s, ULS_MonsterCross);
    }
    else if (!strcmp(prop, "monsteruse"))
    {
      linedef.spac |= UScanFlag(s, ULS_MonsterUse);
    }
    else if (!strcmp(prop, "impact"))
    {
      linedef.spac |= UScanFlag(s, ULS_Impact);
    }
    else if (!strcmp(prop, "playerpush"))
    {
      linedef.spac |= UScanFlag(s, ULS_PlayerPush);
    }
    else if (!strcmp(prop, "monsterpush"))
    {
      linedef.spac |= UScanFlag(s, ULS_MonsterPush);
    }
    else if (!strcmp(prop, "missilecross"))
    {
      linedef.spac |= UScanFlag(s, ULS_MissileCross);
    }
    else if (!strcmp(prop, "repeatspecial"))
    {
      linedef.spac |= UScanFlag(s, ULS_RepeatSpecial);
    }
    else
    {
      UScanSkip(s);
    }
  }

  level.udmf_linedef.push_back(linedef);
}

void ParseUSidedef(Scanner &s, Level &level)
{
  USidedefMinimal sidedef;
  UtilZero(&sidedef);
  sidedef.texturebottom[0] = '-';
  sidedef.texturemiddle[0] = '-';
  sidedef.texturetop[0] = '-';

  s.MustGetToken('{');
  while (!s.CheckToken('}'))
  {
    s.MustGetToken(TK_Identifier);
    auto key = s->str;
    auto prop = key.data();
    CStringToLower(prop);

    if (!strcmp(prop, "offsetx"))
    {
      sidedef.offsetx = UScanInt(s);
    }
    else if (!strcmp(prop, "offsety"))
    {
      sidedef.offsety = UScanInt(s);
    }
    else if (!strcmp(prop, "sector"))
    {
      sidedef.sector_id = static_cast<size_t>(UScanInt(s));
    }
    else if (!strcmp(prop, "texturetop"))
    {
      UScanLumpName(s, sidedef.texturetop);
    }
    else if (!strcmp(prop, "texturemiddle"))
    {
      UScanLumpName(s, sidedef.texturemiddle);
    }
    else if (!strcmp(prop, "texturebottom"))
    {
      UScanLumpName(s, sidedef.texturebottom);
    }
    else
    {
      UScanSkip(s);
    }
  }

  level.udmf_sidedef.push_back(sidedef);
}

void ParseUSector(Scanner &s, Level &level)
{
  USectorMinimal sector;
  UtilZero(&sector);
  sector.lightlevel = 160;
  sector.texturefloor[0] = '-';
  sector.textureceiling[0] = '-';

  s.MustGetToken('{');
  while (!s.CheckToken('}'))
  {
    s.MustGetToken(TK_Identifier);
    auto key = s->str;
    StringToLower(key);
    const char *prop = key.c_str();

    if (!strcmp(prop, "heightfloor"))
    {
      sector.heightfloor = UScanInt(s);
    }
    else if (!strcmp(prop, "heightceiling"))
    {
      sector.heightceiling = UScanInt(s);
    }
    else if (!strcmp(prop, "texturefloor"))
    {
      UScanLumpName(s, sector.texturefloor);
    }
    else if (!strcmp(prop, "textureceiling"))
    {
      UScanLumpName(s, sector.textureceiling);
    }
    else if (!strcmp(prop, "lightlevel"))
    {
      sector.lightlevel = UScanInt(s);
    }
    else if (!strcmp(prop, "special"))
    {
      sector.special = UScanInt(s);
    }
    else if (!strcmp(prop, "id"))
    {
      sector.tag = UScanInt(s);
    }
    else
    {
      UScanSkip(s);
    }
  }

  level.udmf_sector.push_back(sector);
}

void ParseUThing(Scanner &s, Level &level)
{
  UThingMinimal thing;
  UtilZero(&thing);

  s.MustGetToken('{');
  while (!s.CheckToken('}'))
  {
    s.MustGetToken(TK_Identifier);
    auto key = s->str;
    StringToLower(key);
    const char *prop = key.c_str();

    if (!strcmp(prop, "type"))
    {
      thing.type = UScanInt(s);
    }
    else if (!strcmp(prop, "id"))
    {
      thing.tid = UScanInt(s);
    }
    else if (!strcmp(prop, "x"))
    {
      thing.x = UScanFloat(s);
    }
    else if (!strcmp(prop, "y"))
    {
      thing.y = UScanFloat(s);
    }
    else if (!strcmp(prop, "height"))
    {
      thing.height = UScanFloat(s);
    }
    else if (!strcmp(prop, "angle"))
    {
      thing.angle = UScanInt(s);
    }
    else if (!strcmp(prop, "skill1"))
    {
      thing.options |= UScanFlag(s, UTF_Skill1);
    }
    else if (!strcmp(prop, "skill2"))
    {
      thing.options |= UScanFlag(s, UTF_Skill2);
    }
    else if (!strcmp(prop, "skill3"))
    {
      thing.options |= UScanFlag(s, UTF_Skill3);
    }
    else if (!strcmp(prop, "skill4"))
    {
      thing.options |= UScanFlag(s, UTF_Skill4);
    }
    else if (!strcmp(prop, "skill5"))
    {
      thing.options |= UScanFlag(s, UTF_Skill5);
    }
    else if (!strcmp(prop, "ambush"))
    {
      thing.options |= UScanFlag(s, UTF_Ambush);
    }
    else if (!strcmp(prop, "single"))
    {
      thing.options |= UScanFlag(s, UTF_Single);
    }
    else if (!strcmp(prop, "dm"))
    {
      thing.options |= UScanFlag(s, UTF_DM);
    }
    else if (!strcmp(prop, "coop"))
    {
      thing.options |= UScanFlag(s, UTF_Coop);
    }
    else if (!strcmp(prop, "friend"))
    {
      thing.options |= UScanFlag(s, UTF_Friend);
    }
    else if (!strcmp(prop, "dormant"))
    {
      thing.options |= UScanFlag(s, UTF_Dormant);
    }
    else if (!strcmp(prop, "class1"))
    {
      thing.options |= UScanFlag(s, UTF_Class1);
    }
    else if (!strcmp(prop, "class2"))
    {
      thing.options |= UScanFlag(s, UTF_Class2);
    }
    else if (!strcmp(prop, "class3"))
    {
      thing.options |= UScanFlag(s, UTF_Class3);
    }
    else if (!strcmp(prop, "standing"))
    {
      thing.options |= UScanFlag(s, UTF_Standing);
    }
    else if (!strcmp(prop, "strifeally"))
    {
      thing.options |= UScanFlag(s, UTF_StrifeAlly);
    }
    else if (!strcmp(prop, "translucent"))
    {
      thing.options |= UScanFlag(s, UTF_Translucent);
    }
    else if (!strcmp(prop, "invisible"))
    {
      thing.options |= UScanFlag(s, UTF_Invisible);
    }
    else
    {
      UScanSkip(s);
    }
  }

  level.udmf_thing.push_back(thing);
}

//-----------------------------------------------------------------------------

void ParseTextMap(WadIO &io, Level &level, std::vector<char> &lump)
{
  auto s = Scanner(lump.data(), lump.size());

  while (s.TokensLeft())
  {
    s.MustGetToken(TK_Identifier);
    std::string toplevel = s->str;
    StringToLower(toplevel);
    const char *prop = toplevel.c_str();

    if (!strcmp(prop, "namespace"))
    {
      ParseUNamespace(s, level);
    }
    else if (!strcmp(prop, "vertex"))
    {
      ParseUVertex(s, level);
    }
    else if (!strcmp(prop, "linedef"))
    {
      ParseULinedef(s, level);
    }
    else if (!strcmp(prop, "sidedef"))
    {
      ParseUSidedef(s, level);
    }
    else if (!strcmp(prop, "sector"))
    {
      ParseUSector(s, level);
    }
    else if (!strcmp(prop, "thing"))
    {
      ParseUThing(s, level);
    }
    else
    {
      UScanSkip(s);
    }
  }

  if (level.udmf_vertex.size() == 0)
    throw RuntimeException("Not enough UDMF vertexes for map {}. Check your TEXTMAP.", io.LumpName(level.lumps.label));

  if (level.udmf_linedef.size() == 0)
    throw RuntimeException("Not enough UDMF linedefs for map {}. Check your TEXTMAP.", io.LumpName(level.lumps.label));

  if (level.udmf_sidedef.size() == 0)
    throw RuntimeException("Not enough UDMF sidedefs for map {}. Check your TEXTMAP.", io.LumpName(level.lumps.label));

  if (level.udmf_sector.size() == 0)
    throw RuntimeException("Not enough UDMF sectors for map {}. Check your TEXTMAP.", io.LumpName(level.lumps.label));

  if (level.udmf_thing.size() == 0)
    throw RuntimeException("Not enough UDMF things for map {}. Check your TEXTMAP.", io.LumpName(level.lumps.label));
}

void ParseUDMF(WadIO &io, Level &level)
{
  // Get lump data, init parsing
  auto lump = io.ReadLevelLump<char>(level.lumps.label, "TEXTMAP");

  // Now, parse it all
  ParseTextMap(io, level, lump);
}
