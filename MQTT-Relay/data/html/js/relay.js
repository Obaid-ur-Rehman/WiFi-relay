var Relay = (function () {
    function Relay() {
        this.rooturl = "api/";
        this.templates = new Array();
        this.triggers = new Array();
    }
    Relay.prototype.init = function () {
        var setup = this.getUrlParameter("setup");
        if (setup === undefined) {
            $("#pageHeader").html("Стан виходів");
            this.loadTemplateByName("switch", this.loadProcesses);
        }
        else {
            $("#pageHeader").html("Налаштування");
            this.loadSetup(setup, this.getUrlParameter("index"));
        }
    };
    Relay.prototype.loadProcesses = function () {
        $.get(Relay.relay.rooturl + "switches").done(function (data, status) {
            var w = data;
            var list = "";
            for (var i = 0; i < w.items.length; i++) {
                var item = w.items[i];
                if (item.visual) {
                    var t = Relay.relay.getTemplate(item.visual);
                    if (t === undefined) {
                        list += "<li class='nav-item'>";
                        list += item.name;
                        list += "</li>";
                    }
                    else {
                        list += Relay.relay.fillTemplate(t, item);
                    }
                }
            }
            ;
            $(".process-list").html(list);
            $(".switch-checkbox[data-state='ON']").prop("checked", true);
            $("img[data-state='ON']").removeClass("light-off");
            $(".switch-checkbox").change(function () {
                if (this.checked) {
                    Relay.relay.turnOn($(this).data("switch"));
                }
                else {
                    Relay.relay.turnOff($(this).data("switch"));
                }
            });
        }).fail(function (data, status) {
            $(".process-list").html("Не вдалось завантажити. <button class'btn btn-primary refresh'>Повторити</button>");
            $(".switch-checkbox").click(function () {
                Relay.relay.loadProcesses();
            });
        });
    };
    Relay.prototype.getTemplate = function (s) {
        for (var i = 0; i < this.templates.length; i++) {
            var item = this.templates[i];
            if (item.key === s) {
                return item.value;
            }
        }
        ;
        return undefined;
    };
    Relay.prototype.loadTemplateByName = function (name, onDone) {
        $.get(Relay.relay.rooturl + "template?name=" + name).done(function (data, status) {
            var item = new keyValue();
            item.key = name;
            item.value = data;
            Relay.relay.templates.push(item);
            onDone();
        }).fail(function (data, status) {
            $(".process-list").html("Не вдалось завантажити. <button class'btn btn-primary refresh'>Повторити</button>");
            $(".switch-checkbox").click(function () {
                Relay.relay.loadTriggerTemplate();
            });
        });
    };
    Relay.prototype.loadTriggerTemplate = function () {
        var allReady = true;
        for (var i = 0; i < Relay.relay.triggers.length; i++) {
            var item = Relay.relay.triggers[i];
            var t = Relay.relay.getTemplate(item.template);
            if (t === undefined) {
                allReady = false;
                Relay.relay.loadTemplateByName(item.template, Relay.relay.loadTriggerTemplate);
                return;
            }
            t = Relay.relay.getTemplate(item.editingtemplate);
            if (t === undefined) {
                allReady = false;
                Relay.relay.loadTemplateByName(item.editingtemplate, Relay.relay.loadTriggerTemplate);
                return;
            }
        }
        ;
        if (allReady === true) {
            var list = "";
            for (var i = 0; i < Relay.relay.triggers.length; i++) {
                var item = Relay.relay.triggers[i];
                var t = Relay.relay.getTemplate(item.template);
                list += "<div style='display:inline-block;' class='holder' id='trigger_" + item.uid + "' data-index='" + i.toString() + "'>" + Relay.relay.fillTemplate(t, item) + "</div>";
            }
            $(".process-list").html(list);
            $(".btn-edit").click(function (e) {
                Relay.relay.edit(e);
            });
            $(".switch-checkbox[data-action='on']").prop("checked", true);
        }
    };
    Relay.prototype.fillTemplate = function (t, item) {
        var ret = t;
        for (var key in item) {
            var s = item[key];
            var k = "@" + key + "@";
            while (ret.indexOf(k) >= 0) {
                ret = ret.replace(k, s);
            }
        }
        return ret;
    };
    Relay.prototype.edit = function (e) {
        console.log("edit");
        var holder = $(e.target).closest(".holder");
        holder.data("edit", true);
        $('.btn-edit').attr("disabled", "disabled");
        var i = holder.data("index");
        var item = this.triggers[i];
        var t = this.getTemplate(item.editingtemplate);
        holder.html(this.fillTemplate(t, item));
        $(".switch-checkbox[data-action='on']").prop("checked", true);
    };
    Relay.prototype.loadSetup = function (setup, index) {
        $.get(Relay.relay.rooturl + "setup?type=switch&index=" + index.toString()).done(function (data, status) {
            for (var i = 0; i < data.items.length; i++) {
                var item = data.items[i];
                Relay.relay.triggers.push(item);
            }
            ;
            Relay.relay.loadTriggerTemplate();
        }).fail(function (data, status) {
            $(".process-list").html("Не вдалось завантажити. <button class'btn btn-primary refresh'>Повторити</button>");
            $(".switch-checkbox").click(function () {
                Relay.relay.loadSetup(setup, index);
            });
        });
    };
    Relay.prototype.getUrlParameter = function (sParam) {
        var sPageURL = window.location.search.substring(1);
        var sURLVariables = sPageURL.split('&');
        var sParameterName;
        var i;
        for (i = 0; i < sURLVariables.length; i++) {
            sParameterName = sURLVariables[i].split('=');
            if (sParameterName[0] === sParam) {
                return (sParameterName[1] === undefined) ? true : decodeURIComponent(sParameterName[1]);
            }
        }
        return undefined;
    };
    Relay.prototype.turnOn = function (i) {
        if ($("#switch_" + i.toString()).data("state") == "ON")
            return;
        console.log("turn on");
        $.post(Relay.relay.rooturl + "switches" + "?index=" + i.toString() + "&state=on").done(function (data, status) {
            $("#switch_" + i.toString()).data("state", "ON");
            $("#switch_img_" + i.toString()).removeClass("light-off").addClass("light-on");
        }).fail(function (data, status) {
            $("#switch_" + i.toString()).prop("checked", false);
        });
        return true;
    };
    Relay.prototype.turnOff = function (i) {
        if ($("#switch_" + i.toString()).data("state") == "OFF")
            return;
        console.log("turn off");
        $.post(Relay.relay.rooturl + "switches" + "?index=" + i.toString() + "&state=off").done(function (data, status) {
            $("#switch_" + i.toString()).data("state", "OFF");
            $("#switch_img_" + i.toString()).removeClass("light-on").addClass("light-off");
        }).fail(function (data, status) {
            $("#switch_" + i.toString()).prop("checked", true);
        });
        return true;
    };
    Relay.prototype.wifi = function () {
        $.get(Relay.relay.rooturl + "wifi").done(function (data, status) {
            var w = data;
            var list = "";
            for (var i = 0; i < w.ssid.length; i++) {
                list += "<a class='wifi-item list-group-item' href='#'>";
                list += "<div class='name'>";
                list += w.ssid[i].name;
                list += "</div>";
                list += "<div class='encryption'>";
                if (w.ssid[i].encryption == "7")
                    list += "-";
                else
                    list += "***";
                list += "</div>";
                list += "<div class='rssi'>";
                list += w.ssid[i].rssi;
                list += "</div>";
                list += "</a>";
            }
            ;
            $(".wifi-list").html(list);
            $(".wifi-item").on("click", function () {
                $('#ssid').val($(".name", this).text());
                $('#password').focus();
            });
        }).fail(function (data, status) {
            $(".wifi-list").html("Не вдалось завантажити список мереж.<br />Поновіть сторінку, щоб повторити спробу.");
        });
    };
    Relay.relay = new Relay();
    return Relay;
}());
var WIFI_list = (function () {
    function WIFI_list() {
    }
    return WIFI_list;
}());
var Items_list = (function () {
    function Items_list() {
    }
    return Items_list;
}());
var keyValue = (function () {
    function keyValue() {
    }
    return keyValue;
}());
var Trigger = (function () {
    function Trigger() {
    }
    return Trigger;
}());
//# sourceMappingURL=relay.js.map