(function() {
  var DOMContentLoaded, isReady, totoLogin;

  isReady = false;

  totoLogin = {
    ready: function() {
      var head;
      head = document.getElementsByTagName('head')[0];
      head.appendChild(jq);
      return alert("here");
    },
    doScrollCheck: function() {
      if (isReady) return;
      try {
        document.documentElement.doScroll("left");
      } catch (e) {
        setTimeout(totoLogin.doScrollCheck, 1);
        return;
      }
      return ready();
    },
    bindReady: function() {
      var toplevel;
      if (document.readyState === "complete") {
        setTimeout(totoLogin.ready, 1);
        return;
      }
      if (document.addEventListener) {
        document.addEventListener("DOMContentLoaded", DOMContentLoaded, false);
        return window.addEventListener("load", totoLogin.ready, false);
      } else if (document.attachEvent) {
        document.attachEvent("onreadystatechange", DOMContentLoaded);
        window.attachEvent("onload", totoLogin.ready);
        toplevel = false;
        try {
          toplevel = window.frameElement === null;
        } catch (e) {

        }
        if (document.documentElement.doScroll && toplevel) {
          return totoLogin.doScrollCheck();
        }
      }
    }
  };

  if (document.addEventListener) {
    DOMContentLoaded = function() {
      document.removeEventListener("DOMContentLoaded", DOMContentLoaded, false);
      return totoLogin.ready();
    };
  } else if (document.attachEvent) {
    DOMContentLoaded = function() {
      if (document.readyState === "complete") {
        document.detachEvent("onreadystatechange", DOMContentLoaded);
        return totoLogin.ready();
      }
    };
  }

  totoLogin.bindReady();

}).call(this);
