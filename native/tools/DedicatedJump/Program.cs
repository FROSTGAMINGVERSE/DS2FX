// SPDX-License-Identifier: GPL-3.0-or-later
// Local build-time helper linked with SoulsFormats. No Seeker assets are copied.
using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

class Program {
    static void Require(bool condition,string message) { if(!condition) throw new Exception(message); }
    static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
    static void Main(string[] args) {
        Require(args.Length==2,"Usage: DedicatedJump <Vanilla ADP regulation> <new output file>");
        Require(!File.Exists(args[1]),"Output exists; refusing overwrite");
        System.Text.Encoding.RegisterProvider(System.Text.CodePagesEncodingProvider.Instance);
        var input=File.ReadAllBytes(args[0]);
        Require(Hash(input)=="619a0fdd266f2677179460204d875c9ef7dd62af0ba0c70bc2d7511f66e167d3","Unsupported Vanilla ADP regulation hash");
        var binder=BND4.Read(input);
        var before=binder.Files.Select(f=>(f.ID,f.Name,f.Flags,Bytes:f.Bytes.ToArray())).ToArray();
        var entry=binder.Files.Single(f=>f.Name.Replace('\\','/').EndsWith("PlayerCommonParam.param",StringComparison.OrdinalIgnoreCase));
        var param=PARAM.Read(entry.Bytes);
        Require(param.ParamType=="PLAYER_COMMON_PARAM" && !param.BigEndian && param.Rows.Count==1 && param.Rows[0].ID==0,"Unexpected PlayerCommonParam layout");
        // Smithbox's named schema identifies offset 0x1C as jumpTriggerMinSpeed.
        int offset=checked((int)param.Rows[0].DataOffset+0x1c);
        Require(offset>=0 && offset+4<=entry.Bytes.Length,"Invalid field offset");
        Require(BitConverter.ToSingle(entry.Bytes,offset)==4.4f,"Unexpected original jump speed");
        entry.Bytes=entry.Bytes.ToArray();
        Array.Copy(BitConverter.GetBytes(0f),0,entry.Bytes,offset,4);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[1]))!);
        binder.Write(args[1]);
        var result=BND4.Read(args[1]);
        Require(result.Files.Count==before.Length && result.Compression==binder.Compression && result.Version==binder.Version && result.Format==binder.Format,"Binder layout changed");
        int changedEntries=0;
        for(int i=0;i<before.Length;i++) {
            var old=before[i];var now=result.Files[i];
            Require(old.ID==now.ID && old.Name==now.Name && old.Flags==now.Flags,"Entry identity changed");
            if(old.Bytes.SequenceEqual(now.Bytes)) continue;
            changedEntries++;
            Require(now.Name==entry.Name && now.Bytes.Length==old.Bytes.Length,"Unrelated content changed");
            for(int n=0;n<old.Bytes.Length;n++) if(n<offset || n>=offset+4) Require(old.Bytes[n]==now.Bytes[n],"Byte outside jump field changed");
            Require(BitConverter.ToSingle(now.Bytes,offset)==0f,"Jump value not applied");
        }
        Require(changedEntries==1,"Expected exactly one changed binder entry");
        var audit=new { input_sha256=Hash(input),output_sha256=Hash(File.ReadAllBytes(args[1])),entry=entry.Name,row=0,field="jumpTriggerMinSpeed",row_offset="0x1C",file_offset=offset,before=4.4f,after=0f,total_entries=before.Length,changed_entries=changedEntries,unchanged_entries=before.Length-changedEntries,scope="Only four bytes in PlayerCommonParam; every other entry byte-identical; no Seeker assets imported" };
        var json=JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true});
        File.WriteAllText(args[1]+".audit.json",json);Console.WriteLine(json);
    }
}
