isReady = false

totoLogin =
  ready: ->
    head = document.getElementsByTagName('head')[0]
    head.appendChild(jq);
    alert("here")

  doScrollCheck: ->
  	if isReady
  		return;

  	try
  		# If IE is used, use the trick by Diego Perini
  		# http://javascript.nwbox.com/IEContentLoaded/
  		document.documentElement.doScroll("left");
  	catch e
  		setTimeout totoLogin.doScrollCheck, 1
  		return

  	# and execute any waiting functions
  	ready()

  bindReady: ->
    # Catch cases where $(document).ready() is called after the
    # browser event has already occurred.
    if document.readyState is "complete"
      # Handle it asynchronously to allow scripts the opportunity to delay ready
      setTimeout totoLogin.ready, 1
      return

    # Mozilla, Opera and webkit nightlies currently support this event
    if document.addEventListener
      # Use the handy event callback
      document.addEventListener "DOMContentLoaded", DOMContentLoaded, false
      # A fallback to window.onload, that will always work
      window.addEventListener "load", totoLogin.ready, false

    # If IE event model is used
    else if document.attachEvent
      # ensure firing before onload,
      # maybe late but safe also for iframes
      document.attachEvent "onreadystatechange", DOMContentLoaded

      # A fallback to window.onload, that will always work
      window.attachEvent "onload", totoLogin.ready

      # If IE and not a frame
      # continually check to see if the document is ready
      toplevel = false

      try
        toplevel = window.frameElement == null;
      catch e

      if document.documentElement.doScroll and toplevel
        totoLogin.doScrollCheck()

if document.addEventListener
	DOMContentLoaded = ->
		document.removeEventListener "DOMContentLoaded", DOMContentLoaded, false
		totoLogin.ready()
else if document.attachEvent
	DOMContentLoaded = ->
		# Make sure body exists, at least, in case IE gets a little overzealous (ticket #5443).
		if document.readyState is "complete"
			document.detachEvent "onreadystatechange", DOMContentLoaded
			totoLogin.ready()

totoLogin.bindReady()