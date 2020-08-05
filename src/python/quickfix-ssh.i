
#define HAVE_SSL       1

%include ./quickfix.i

%pythoncode %{
#ifdef SWIGPYTHON

class SSLSocketInitiator(SSLSocketInitiatorBase):
  application = 0
  storeFactory = 0
  setting = 0
  logFactory = 0

  def __init__(self, application, storeFactory, settings, logFactory=None):
    if logFactory == None:
      SSLSocketInitiatorBase.__init__(self, application, storeFactory, settings)
    else:
      SSLSocketInitiatorBase.__init__(self, application, storeFactory, settings, logFactory)

    self.application = application
    self.storeFactory = storeFactory
    self.settings = settings
    self.logFactory = logFactory

class SSLSocketAcceptor(SSLSocketAcceptorBase):
  application = 0
  storeFactory = 0
  setting = 0
  logFactory = 0

  def __init__(self, application, storeFactory, settings, logFactory=None):
    if logFactory == None:
      SSLSocketAcceptorBase.__init__(self, application, storeFactory, settings)
    else:
      SSLSocketAcceptorBase.__init__(self, application, storeFactory, settings, logFactory)

    self.application = application
    self.storeFactory = storeFactory
    self.settings = settings
    self.logFactory = logFactory

#endif
%}
