﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc;
using WebAdmin.Models;
using System.Runtime.InteropServices;

namespace WebAdmin.Controllers
{
    public class ApiController : Controller
    {

        public class MQTTProcess
        {
            public string name { get; set; }
            public string visual { get; set; } = "switch";
            public string type { get; set; } = "switch";
            public string state { get; set; } = "OFF";
            public string index { get; set; } = "0";
        }

        private static List<MQTTProcess> items;
        private IHostingEnvironment _env;
        public ApiController(IHostingEnvironment env)
        {
            _env = env;
            if (items == null)
            {
                items = new List<MQTTProcess>();
                items.Add(new MQTTProcess() { name = "out1", index = "1" });
                items.Add(new MQTTProcess() { name = "out2", index = "2" });
                items.Add(new MQTTProcess() { name = "out3", index = "3" });
                items.Add(new MQTTProcess() { name = "led", index = "4", state = "ON" });
            }

        }

        public IActionResult Index()
        {
            var webRoot = _env.WebRootPath;
            var file = System.IO.Path.Combine(webRoot, "test.txt");
            System.IO.File.WriteAllText(file, "Hello World!");
            return View();
        }
        public IActionResult Get()
        {
            var webRoot = _env.WebRootPath + "\\content\\";

            if (System.IO.Directory.Exists(webRoot))
            {
                string ret = "";
                string var = null;
                String content = System.IO.File.ReadAllText(webRoot + Request.Query["name"] + ".txt");
                int i = 0;
                while (i < content.Length)
                {
                    char c = content[i];
                    if (var == null)
                    {
                        if (c == '[') { var = ""; } else { ret += c; }
                    }
                    else
                    {
                        if (c == ']')
                        {
                            ret += Request.Query[var];
                            var = null;
                        }
                        else { var += c; }
                    }
                    i++;
                };
                return Content(ret);
            }
            return NotFound();
        }

        public IActionResult wifi()
        {
            return Content("{\"ssid\":[{\"name\":\"KOLUMBUS\",\"encryption\":\"4\",\"rssi\":\"-55\"},{\"name\":\"netis_5D1566\",\"encryption\":\"8\",\"rssi\":\"-94\"},{\"name\":\"Ukrtelecom_105\",\"encryption\":\"2\",\"rssi\":\"-64\"},{\"name\":\"TODD0205\",\"encryption\":\"4\",\"rssi\":\"-71\"},{\"name\":\"codename47\",\"encryption\":\"4\",\"rssi\":\"-88\"},{\"name\":\"Prosvity19_98\",\"encryption\":\"4\",\"rssi\":\"-52\"},{\"name\":\"KushlaVR\",\"encryption\":\"4\",\"rssi\":\"-55\"},{\"name\":\"anastasia\",\"encryption\":\"8\",\"rssi\":\"-61\"},{\"name\":\"TP-LINK_44C0\",\"encryption\":\"4\",\"rssi\":\"-76\"},{\"name\":\"TP-LINK_zyzja\",\"encryption\":\"7\",\"rssi\":\"-76\"},{\"name\":\"lyudmyla\",\"encryption\":\"4\",\"rssi\":\"-78\"},{\"name\":\"Volia_93\",\"encryption\":\"4\",\"rssi\":\"-74\"},{\"name\":\"TerNet99\",\"encryption\":\"8\",\"rssi\":\"-72\"},{\"name\":\"Oksana\",\"encryption\":\"8\",\"rssi\":\"-83\"},{\"name\":\"Bandera\",\"encryption\":\"4\",\"rssi\":\"-89\"},{\"name\":\"Svitlana_Wi-Fi\",\"encryption\":\"8\",\"rssi\":\"-83\"}]}", new Microsoft.Net.Http.Headers.MediaTypeHeaderValue("application/json"));
        }


        public IActionResult wifisave(string n, string p)
        {
            return new JsonResult(new { result = "ok" });
        }

        [HttpGet()]
        public IActionResult switches()
        {
            //return Content("{\"items\":[{\"name\":\"out1\",\"type\":\"switch\",\"state\":\"OFF\",\"index\":\"1\",\"visual\":\"switch\"},{\"name\":\"out2\",\"type\":\"switch\",\"state\":\"OFF\",\"index\":\"2\",\"visual\":\"switch\"},{\"name\":\"out3\",\"type\":\"switch\",\"state\":\"OFF\",\"index\":\"3\",\"visual\":\"switch\"},{\"name\":\"led\",\"type\":\"switch\",\"state\":\"ON\",\"index\":\"4\",\"visual\":\"switch\"}]}", new Microsoft.Net.Http.Headers.MediaTypeHeaderValue("application/json"));
            return new JsonResult(new { items = items });
        }

        [HttpPost()]
        public IActionResult switches(int? index, string state)
        {
            if (index.HasValue && !string.IsNullOrEmpty(state))
            {
                if (index.Value <= items.Count)
                {
                    items[index.Value - 1].state = state.ToUpper();
                    return new JsonResult(new { status = "OK" });
                }
            }
            return NotFound();
        }


    }
}
