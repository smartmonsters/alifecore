HEADERS += \
    src/addrdb.h src/addrman.h src/amount.h src/arith_uint256.h src/auxpow.h src/base58.h \
    src/blockencodings.h src/bloom.h src/chain.h src/chainparamsbase.h src/chainparams.h \
    src/chainparamsseeds.h src/checkpoints.h src/checkqueue.h src/clientversion.h src/coincontrol.h \
    src/coins.h src/compat.h src/compressor.h src/core_io.h src/core_memusage.h src/dbwrapper.h \
    src/hash.h src/httprpc.h src/httpserver.h src/indirectmap.h src/init.h src/key.h src/keystore.h \
    src/limitedmap.h src/main.h src/memusage.h src/merkleblock.h src/miner.h src/netaddress.h \
    src/netbase.h src/net.h src/noui.h src/pow.h src/prevector.h src/protocol.h src/pubkey.h src/random.h \
    src/reverselock.h src/scheduler.h src/serialize.h src/streams.h src/sync.h src/threadsafety.h \
    src/timedata.h src/tinyformat.h src/torcontrol.h src/txdb.h src/txmempool.h src/ui_interface.h \
    src/uint256.h src/undo.h src/util.h src/utilmoneystr.h src/utilstrencodings.h src/utiltime.h \
    src/validationinterface.h src/versionbits.h src/version.h \
    src/consensus/consensus.h src/consensus/merkle.h src/consensus/params.h src/consensus/validation.h \
    src/game/common.h \
    src/game/db.h \
    src/game/map.h \
    src/game/move.h \
    src/game/movecreator.h \
    src/game/state.h \
    src/game/tx.h \
    src/names/common.h \
    src/names/main.h \
    src/policy/fees.h \
    src/policy/policy.h \
    src/policy/rbf.h \
    src/primitives/block.h \
    src/primitives/pureheader.h \
    src/primitives/transaction.h \
    src/qt/addressbookpage.h src/qt/addresstablemodel.h src/qt/askpassphrasedialog.h src/qt/bantablemodel.h \
    src/qt/bitcoinaddressvalidator.h src/qt/bitcoinamountfield.h src/qt/bitcoingui.h src/qt/bitcoinunits.h \
    src/qt/clientmodel.h src/qt/coincontroldialog.h src/qt/coincontroltreewidget.h src/qt/csvmodelwriter.h \
    src/qt/editaddressdialog.h src/qt/guiconstants.h src/qt/guiutil.h src/qt/intro.h src/qt/macdockiconhandler.h \
    src/qt/macnotificationhandler.h src/qt/modaloverlay.h src/qt/networkstyle.h src/qt/notificator.h \
    src/qt/openuridialog.h src/qt/optionsdialog.h src/qt/optionsmodel.h src/qt/overviewpage.h \
    src/qt/paymentrequestplus.h src/qt/paymentserver.h src/qt/peertablemodel.h src/qt/platformstyle.h \
    src/qt/qvalidatedlineedit.h src/qt/qvaluecombobox.h src/qt/receivecoinsdialog.h \
    src/qt/receiverequestdialog.h src/qt/recentrequeststablemodel.h src/qt/rpcconsole.h \
    src/qt/sendcoinsdialog.h src/qt/sendcoinsentry.h src/qt/signverifymessagedialog.h src/qt/splashscreen.h \
    src/qt/trafficgraphwidget.h src/qt/transactiondescdialog.h src/qt/transactiondesc.h \
    src/qt/transactionfilterproxy.h src/qt/transactionrecord.h src/qt/transactiontablemodel.h \
    src/qt/transactionview.h src/qt/utilitydialog.h src/qt/walletframe.h src/qt/walletmodel.h \
    src/qt/walletmodeltransaction.h src/qt/walletview.h src/qt/winshutdownmonitor.h \
    src/rpc/client.h src/rpc/protocol.h src/rpc/register.h src/rpc/server.h \
    src/script/interpreter.h src/script/ismine.h src/script/namecoinconsensus.h src/script/names.h src/script/script_error.h src/script/script.h\
    src/script/sigcache.h src/script/sign.h src/script/standard.h \
    src/wallet/crypter.h src/wallet/db.h src/wallet/rpcwallet.h src/wallet/walletdb.h src/wallet/wallet.h \
    src/zmq/zmqabstractnotifier.h src/zmq/zmqconfig.h src/zmq/zmqnotificationinterface.h \
    src/zmq/zmqpublishnotifier.h

SOURCES += \
    src/addrdb.cpp src/addrman.cpp src/amount.cpp src/arith_uint256.cpp src/auxpow.cpp src/base58.cpp \
    src/bitcoin-cli.cpp src/bitcoind.cpp src/bitcoin-tx.cpp src/blockencodings.cpp src/bloom.cpp \
    src/chain.cpp src/chainparamsbase.cpp src/chainparams.cpp src/checkpoints.cpp \
    src/clientversion.cpp src/coins.cpp src/compressor.cpp src/core_read.cpp src/core_write.cpp \
    src/dbwrapper.cpp src/hash.cpp src/httprpc.cpp src/httpserver.cpp src/init.cpp src/key.cpp \
    src/keystore.cpp src/main.cpp src/merkleblock.cpp src/miner.cpp src/netaddress.cpp src/netbase.cpp \
    src/net.cpp src/noui.cpp src/pow.cpp src/protocol.cpp src/pubkey.cpp src/random.cpp src/rest.cpp \
    src/scheduler.cpp src/sync.cpp src/timedata.cpp src/torcontrol.cpp src/txdb.cpp src/txmempool.cpp \
    src/ui_interface.cpp src/uint256.cpp src/util.cpp src/utilmoneystr.cpp src/utilstrencodings.cpp \
    src/utiltime.cpp src/validationinterface.cpp src/versionbits.cpp \
    src/consensus/merkle.cpp \
    src/game/common.cpp \
    src/game/db.cpp \
    src/game/map.cpp \
    src/game/move.cpp \
    src/game/movecreator.cpp \
    src/game/state.cpp \
    src/game/tx.cpp \
    src/names/common.cpp \
    src/names/main.cpp \
    src/policy/fees.cpp \
    src/policy/policy.cpp \
    src/policy/rbf.cpp \
    src/primitives/block.cpp \
    src/primitives/pureheader.cpp \
    src/primitives/transaction.cpp \
    src/qt/addressbookpage.cpp src/qt/addresstablemodel.cpp src/qt/askpassphrasedialog.cpp \
    src/qt/bantablemodel.cpp src/qt/bitcoinaddressvalidator.cpp src/qt/bitcoinamountfield.cpp \
    src/qt/bitcoin.cpp src/qt/bitcoingui.cpp src/qt/bitcoinstrings.cpp src/qt/bitcoinunits.cpp \
    src/qt/clientmodel.cpp src/qt/coincontroldialog.cpp src/qt/coincontroltreewidget.cpp \
    src/qt/csvmodelwriter.cpp src/qt/editaddressdialog.cpp src/qt/guiutil.cpp src/qt/intro.cpp \
    src/qt/modaloverlay.cpp src/qt/networkstyle.cpp src/qt/notificator.cpp src/qt/openuridialog.cpp \
    src/qt/optionsdialog.cpp src/qt/optionsmodel.cpp src/qt/overviewpage.cpp src/qt/paymentrequestplus.cpp \
    src/qt/paymentserver.cpp src/qt/peertablemodel.cpp src/qt/platformstyle.cpp \
    src/qt/qvalidatedlineedit.cpp src/qt/qvaluecombobox.cpp src/qt/receivecoinsdialog.cpp \
    src/qt/receiverequestdialog.cpp src/qt/recentrequeststablemodel.cpp src/qt/rpcconsole.cpp \
    src/qt/sendcoinsdialog.cpp src/qt/sendcoinsentry.cpp src/qt/signverifymessagedialog.cpp \
    src/qt/splashscreen.cpp src/qt/trafficgraphwidget.cpp src/qt/transactiondesc.cpp \
    src/qt/transactiondescdialog.cpp src/qt/transactionfilterproxy.cpp src/qt/transactionrecord.cpp \
    src/qt/transactiontablemodel.cpp src/qt/transactionview.cpp src/qt/utilitydialog.cpp \
    src/qt/walletframe.cpp src/qt/walletmodel.cpp src/qt/walletmodeltransaction.cpp src/qt/walletview.cpp \
    src/qt/winshutdownmonitor.cpp \
    src/rpc/blockchain.cpp src/rpc/client.cpp src/rpc/game.cpp src/rpc/mining.cpp src/rpc/misc.cpp src/rpc/names.cpp src/rpc/net.cpp \
    src/rpc/protocol.cpp src/rpc/rawtransaction.cpp src/rpc/server.cpp \
    src/script/interpreter.cpp src/script/ismine.cpp src/script/namecoinconsensus.cpp src/script/names.cpp src/script/script.cpp \
    src/script/script_error.cpp src/script/sigcache.cpp src/script/sign.cpp src/script/standard.cpp \
    src/wallet/crypter.cpp src/wallet/db.cpp src/wallet/rpcdump.cpp src/wallet/rpcnames.cpp src/wallet/rpcwallet.cpp src/wallet/wallet.cpp \
    src/wallet/walletdb.cpp \
    src/zmq/zmqabstractnotifier.cpp src/zmq/zmqnotificationinterface.cpp src/zmq/zmqpublishnotifier.cpp

