// SPDX-License-Identifier: GPL-3.0-or-later
// Local adapter: no assets from Nexus 1473 are copied or required.
using System;using System.IO;using System.Linq;using System.Security.Cryptography;using System.Text.Json;using SoulsFormats;
class Program {
 static void Require(bool ok,string message){if(!ok)throw new Exception(message);}
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
 static void Main(string[] args){
  Require(args.Length==2,"Usage: EstusMovement <existing regulation> <new walking-off regulation>");
  Require(!File.Exists(args[1]),"Output exists");
  System.Text.Encoding.RegisterProvider(System.Text.CodePagesEncodingProvider.Instance);
  var input=File.ReadAllBytes(args[0]);var hash=Hash(input);
  Require(new[]{"619a0fdd266f2677179460204d875c9ef7dd62af0ba0c70bc2d7511f66e167d3","6f9194cc1e6c439cb5502da17c4ac2f8ffacc5123ee9e6043482179936a3e2e9"}.Contains(hash),"Unsupported baseline");
  var binder=BND4.Read(input);var before=binder.Files.Select(f=>(f.ID,f.Name,f.Flags,Bytes:f.Bytes.ToArray())).ToArray();
  var entry=binder.Files.Single(f=>f.Name.Replace('\\','/').EndsWith("ItemTypeParam.param",StringComparison.OrdinalIgnoreCase));
  var p=PARAM.Read(entry.Bytes);Require(p.ParamType=="ITEM_TYPE_PARAM"&&!p.BigEndian,"Wrong PARAM layout");
  var row=p.Rows.Single(r=>r.ID==450);int offset=checked((int)row.DataOffset+4);
  Require(BitConverter.ToSingle(entry.Bytes,offset)==0.5f,"Unexpected existing Estus movement");
  entry.Bytes=entry.Bytes.ToArray();Array.Copy(BitConverter.GetBytes(0f),0,entry.Bytes,offset,4);
  Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[1]))!);binder.Write(args[1]);
  var result=BND4.Read(args[1]);Require(result.Files.Count==before.Length&&result.Compression==binder.Compression&&result.Version==binder.Version&&result.Format==binder.Format,"Archive layout changed");
  int changed=0;for(int i=0;i<before.Length;i++){
   var old=before[i];var now=result.Files[i];Require(old.ID==now.ID&&old.Name==now.Name&&old.Flags==now.Flags,"Entry identity changed");
   if(old.Bytes.SequenceEqual(now.Bytes))continue;changed++;
   Require(now.Name==entry.Name&&now.Bytes.Length==old.Bytes.Length,"Unrelated entry changed");
   for(int n=0;n<old.Bytes.Length;n++)if(n<offset||n>=offset+4)Require(old.Bytes[n]==now.Bytes[n],"Unrelated field changed");
   Require(BitConverter.ToSingle(now.Bytes,offset)==0f,"Walking-off patch failed");
  }
  Require(changed==1,"Expected one changed entry");
  var audit=new{input_sha256=hash,output_sha256=Hash(File.ReadAllBytes(args[1])),entry=entry.Name,row=450,field="itemUseMaxSpeed",row_offset=4,file_offset=offset,before=0.5f,after=0f,total_entries=before.Length,changed_entries=changed,unchanged_entries=before.Length-changed,scope="Only Estus movement multiplier changed. Other items, timings, healing and jump preserved. Walking ON uses exact pre-update files.",schema="https://github.com/vawser/Smithbox/blob/main/src/Smithbox.Data/Assets/PARAM/DS2S/Defs/ITEM_TYPE_PARAM.xml"};
  var json=JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true});File.WriteAllText(args[1]+".audit.json",json);Console.WriteLine(json);
 }
}
