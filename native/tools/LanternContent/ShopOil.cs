using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Builds a non-deployable shop candidate from existing merchant consumable rows.
// The caller must verify TalkESD range ownership before staging the result.
static class ShopOil {
    const int RepairPowder=60410000, Oil=60425000, Price=100;
    static void Require(bool value,string message){if(!value)throw new InvalidDataException(message);}
    static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes));
    static readonly FieldInfo Offset=typeof(PARAM.Row).GetField("DataOffset",BindingFlags.Instance|BindingFlags.NonPublic)!;
    static PARAM Decode(BND4 bnd,string name,Dictionary<string,PARAMDEF> defs) { var entry=bnd.Files.Single(f=>Path.GetFileName(f.Name.Replace('\\','/'))==name);var param=PARAM.Read(entry.Bytes);param.ApplyParamdef(defs[param.ParamType]);return param; }
    static BinderFile Entry(BND4 bnd,string name)=>bnd.Files.Single(f=>Path.GetFileName(f.Name.Replace('\\','/'))==name);
    static Dictionary<string,PARAMDEF> Definitions(string directory)=>Directory.GetFiles(directory,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(x=>x.ParamType);
    static object Row(PARAM.Row row)=>new {row.ID,Sold=Convert.ToInt32(row["saleItem"].Value),Price=Convert.ToInt32(row["price"].Value),Start=Convert.ToInt32(row["saleFlag"].Value),End=Convert.ToInt32(row["saleEndFlag"].Value),Order=Convert.ToUInt32(row["displayOrder"].Value),Quantity=Convert.ToSByte(row["unitSales"].Value)};
    public static void Inspect(string input,string schemaDirectory) {
        var bnd=BND4.Read(input);var shop=Decode(bnd,"ShopLineupParam.param",Definitions(schemaDirectory));
        var repairPowder=shop.Rows.Where(r=>Convert.ToInt32(r["saleItem"].Value)==RepairPowder).Select(r=>new {RepairPowder=Row(r),Before=shop.Rows.Where(n=>n.ID<r.ID).OrderByDescending(n=>n.ID).Take(2).Reverse().Select(Row),After=shop.Rows.Where(n=>n.ID>r.ID).OrderBy(n=>n.ID).Take(2).Select(Row)}).ToArray();
        Console.WriteLine(JsonSerializer.Serialize(new {input_sha256=Hash(File.ReadAllBytes(input)),param=shop.ParamType,rows=shop.Rows.Count,repair_powder_rows=repairPowder,sample_rows=shop.Rows.OrderBy(r=>r.ID).Take(80).Select(Row)},new JsonSerializerOptions{WriteIndented=true}));
    }
    public static void Build(string input,string schemaDirectory,string output) {
        Require(!File.Exists(output),"Output exists; use a fresh candidate"); var before=File.ReadAllBytes(input); var bnd=BND4.Read(before); var original=bnd.Files.Select(f=>(f.ID,f.Name,f.Flags,f.CompressionType,Bytes:f.Bytes.ToArray())).ToArray(); var defs=Definitions(schemaDirectory);
        var item=Decode(bnd,"ItemParam.param",defs);Require(item.Rows.Any(r=>r.ID==Oil),"Lantern Oil item row is absent; run the existing lantern content builder first");
        var shop=Decode(bnd,"ShopLineupParam.param",defs);Require(shop.Rows.All(r=>Convert.ToInt32(r["saleItem"].Value)!=Oil),"Oil shop rows already exist");
        var repairPowder=shop.Rows.Where(r=>Convert.ToInt32(r["saleItem"].Value)==RepairPowder).OrderBy(r=>r.ID).ToArray();Require(repairPowder.Length>0,"No Repair Powder merchant rows found");var added=new List<object>();
        foreach(var source in repairPowder) {
            // Every clone stays immediately after the source row. A TalkESD range that
            // exposes this merchant row may expose oil without modifying dialogue/ESD.
            int id=source.ID+1;while(shop.Rows.Any(r=>r.ID==id))id++;
            var clone=new PARAM.Row(id,source.Name,defs[shop.ParamType]);for(int i=0;i<source.Cells.Count;i++)clone.Cells[i].Value=source.Cells[i].Value is byte[] bytes?bytes.ToArray():source.Cells[i].Value;
            clone["saleItem"].Value=Oil;clone["price"].Value=Price;clone["unitSales"].Value=(sbyte)-1;
            shop.Rows.Add(clone);added.Add(new {source_id=source.ID,oil_row=id,price=Price,source_conditions=Row(source)});
        }
        shop.Rows=shop.Rows.OrderBy(r=>r.ID).ToList();Entry(bnd,"ShopLineupParam.param").Bytes=shop.Write();var reread=PARAM.Read(Entry(bnd,"ShopLineupParam.param").Bytes);reread.ApplyParamdef(defs[reread.ParamType]);
        Require(reread.Rows.Count==shop.Rows.Count&&reread.Rows.Count(r=>Convert.ToInt32(r["saleItem"].Value)==Oil)==repairPowder.Length,"Shop round-trip lost oil rows");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);bnd.Write(output);var check=BND4.Read(output);foreach(var old in original){var now=check.Files.Single(f=>f.ID==old.ID);Require(now.Name==old.Name&&now.Flags==old.Flags&&now.CompressionType==old.CompressionType,"Binder identity changed");if(now.Name!=Entry(bnd,"ShopLineupParam.param").Name)Require(now.Bytes.SequenceEqual(old.Bytes),"Unrelated binder entry changed: "+old.Name);}
        var audit=new {role="lantern-oil-shop-candidate",input_sha256=Hash(before),output_sha256=Hash(File.ReadAllBytes(output)),oil_item=Oil,price=Price,repair_powder_rows=repairPowder.Length,added,scope="Candidate only: clones each existing Repair Powder merchant row, preserves its availability conditions, and changes only item, price, and infinite stock. It is not deployable until its TalkESD range is verified.",deployed=false};File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine(JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));
    }
}
