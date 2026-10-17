using SoulsFormats;
using System.Text.Json;

// Read-only diagnostics for comparing a small number of ItemParam rows while
// designing a content candidate. This command never writes a binder.
static class ItemInspector
{
    public static void Print(string regulation, string schemaDirectory, int rowId)
    {
        var binder=BND4.Read(regulation);
        var entry=binder.Files.Single(file=>Path.GetFileName(file.Name.Replace('\\','/'))=="ItemParam.param");
        var defs=Directory.GetFiles(schemaDirectory,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def=>def.ParamType);
        var param=PARAM.Read(entry.Bytes);param.ApplyParamdef(defs[param.ParamType]);
        var row=param.Rows.Single(item=>item.ID==rowId);
        var values=row.Cells.Where(cell=>cell.Value is not byte[]).ToDictionary(cell=>cell.Def.InternalName,cell=>cell.Value);
        Console.WriteLine(JsonSerializer.Serialize(new {row.ID,row.Name,Values=values},new JsonSerializerOptions {WriteIndented=true}));
    }

    public static void PrintActiveEffect(string regulation, int eventId)
    {
        var binder=BND4.Read(regulation);
        var entry=binder.Files.Single(file=>Path.GetFileName(file.Name.Replace('\\','/'))=="SpEffectActiveItem.emevd");
        var eventData=EMEVD.Read(entry.Bytes).Events.SingleOrDefault(item=>item.ID==eventId);
        Console.WriteLine(JsonSerializer.Serialize(new {
            EventId=eventId,
            Present=eventData is not null,
            Instructions=eventData?.Instructions.Select(instruction=>new {
                instruction.Bank,instruction.ID,ArgData=Convert.ToHexString(instruction.ArgData)
            })
        },new JsonSerializerOptions {WriteIndented=true}));
    }
}
