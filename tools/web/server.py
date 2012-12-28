#!/usr/bin/env python

import os, platform, urlparse, BaseHTTPServer, threading, time, serial, select, socket, cgi, subprocess, struct
import json
from settings import *
from mote import *
from graph_data import *

motes = []

listenTxt = []
isListening = False
isGraphing = False
listenThread = None

def listenSerial():
    global listenTxt
    while isListening:
        for m in motes:
            m.tryRead()
            while '\n' in m.buffer:
                pos = m.buffer.find('\n')
                if pos != 0:
                    newString = m.buffer[:pos].strip()
                    # print "got", newString
                    listenTxt.append(newString)
                    graphData.addNewData(newString)
                m.buffer = m.buffer[pos + 1:]
        # use only last 28 lines of all motes
        listenTxt = listenTxt[-28:]
        # use only last 40 graph readings
        graphData.resize(40)
        # pause for a bit
        time.sleep(0.1)


def closeAllSerial():
    global listenThread
    global isListening
    isListening = False
    if listenThread:
        listenThread.join()
        listenThread = None
    for m in motes:
        m.closeSerial()


def openAllSerial():
    global listenThread
    global isListening
    global listenTxt
    listenTxt = []
    graphData.reset()
    if isListening: return
    isListening = True
    listenThread = threading.Thread(target=listenSerial)
    listenThread.start()
    for m in motes:
        m.openSerial()

# --------------------------------------------

dataIteration = 0

def getMansosVersion():
    path = settingsInstance.getCfgValue("pathToMansOS")
    result = ""
    try:
        with open(os.path.join(path, "doc/VERSION")) as versionFile:
            result = versionFile.readline().strip()
    except Exception as e:
        print e
    return result

class HttpServerHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    server_version = 'MansOS/' + getMansosVersion() + ' Web Server'
    protocol_version = 'HTTP/1.1' # 'HTTP/1.0' is the default, but we want chunked encoding

    def writeChunk(self, buffer):
        if self.wfile == None: return
        if self.wfile._sock == None: return
        self.wfile.write("{:x}\r\n".format(len(buffer)))
        self.wfile.write(buffer)
        self.wfile.write("\r\n")

    def writeFinalChunk(self):
        self.wfile.write("0\r\n")
        self.wfile.write("\r\n")

    def serveHeader(self, name, isGeneric = True, replaceValues = None):
        with open(settingsInstance.getCfgValue("htmlDirectory") + "/header.html", "r") as f:
            contents = f.read()
            contents = contents.replace("%PAGETITLE%", name)
            self.writeChunk(contents)
        try:
            with open(settingsInstance.getCfgValue("htmlDirectory") + "/" + name + ".header.html", "r") as f:
                contents = f.read()
                if replaceValues:
                    for v in replaceValues:
                        contents = contents.replace("%" + v + "%", replaceValues[v])
                self.writeChunk(contents)
        except:
            pass

        suffix = "generic" if isGeneric else "mote"

        with open(settingsInstance.getCfgValue("htmlDirectory") + "/bodystart-" + suffix + ".html", "r") as f:
            contents = f.read()
            self.writeChunk(contents)


    def serveBody(self, name, replaceValues = None):
        with open(settingsInstance.getCfgValue("htmlDirectory") + "/" + name + ".html", "r") as f:
            contents = f.read()
            if replaceValues:
                for v in replaceValues:
                    contents = contents.replace("%" + v + "%", replaceValues[v])
            self.writeChunk(contents)


    def serveMotes(self, action, qs, isPost):
        if isPost:
            self.writeChunk('<form method="post" enctype="multipart/form-data">\n')
        else:
            self.writeChunk("<form>\n")
        c = ""
        i = 0
        for m in motes:
            name = "mote" + str(i)
            isChecked = qs.get(name)
            if isChecked: isChecked = isChecked[0] == 'on'
            if isChecked:
                m.performAction = True
            else:
                m.performAction = False
            isChecked = ' checked="checked"' if isChecked else ""
            c += '<p class="module"><strong>Mote: </strong>' + m.portName \
                + ' <input type="checkbox" name="' + name + '"' + isChecked + '>' + action + '</p>\n'
            i += 1
        if c:
            self.writeChunk("<br/>Directly attached motes:\n<br/>\n" + c + "<hr/>\n")

    def serveFooter(self):
        with open(settingsInstance.getCfgValue("htmlDirectory") + "/footer.html", "r") as f:
            contents = f.read()
            self.writeChunk(contents)
        self.writeFinalChunk()

    def sendDefaultHeaders(self):
        self.send_header('Content-Type', 'text/html')
        # use chunked transfer encoding (to be able to send additional chunks 'later')
        self.send_header('Transfer-Encoding', 'chunked')
        # disable caching
        self.send_header('Cache-Control', 'no-store');

    def serveDefault(self):
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        self.serveHeader("default")
        self.serveBody("default")
        self.serveFooter()

    def serveMoteSelect(self, qs):
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        self.serveHeader("motes")
        self.serveBody("motes")
        self.serveFooter()

    def serveConfig(self, qs):
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        self.serveHeader("config", False)
        self.serveBody("config")
        self.serveFooter()

    def serveFiles(self, qs):
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        self.serveHeader("files", False)
        self.serveBody("files")
        self.serveFooter()

    def serveGraphs(self, qs):
        global isGraphing
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        self.serveHeader("graphs")
        self.serveMotes("Graph", qs, False)

        if "action" in qs:
            if qs["action"][0] == "start":
                openAllSerial()
                isGraphing = True
            else:
                isGraphing = False
                closeAllSerial()

        if isGraphing:
            listenCmd = "Stop graphing"
            action = "stop"
        else:
            listenCmd = "Start graphing"
            action = "start"

        self.serveBody("graphs",
                       {"GRAPHS_ACTION": action,
                        "GRAPHS_CMD" : listenCmd})
        self.serveFooter()

    def serveListen(self, qs):
        print "serveListen, qs=", qs
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        self.serveHeader("listen")
        self.serveMotes("Listen", qs, False)
        if "action" in qs:
            if qs["action"][0] == "start":
                openAllSerial()
            else:
                closeAllSerial()

        if isListening:
            listenCmd = "Stop listening"
            action = "stop"
        else:
            listenCmd = "Start listening"
            action = "start"
        txt = ""
        for line in listenTxt:
            txt += "&nbsp;&nbsp;&nbsp;&nbsp;" + line + "<br/>"
        self.serveBody("listen",
                       {"LISTEN_TXT" : txt,
                        "LISTEN_ACTION": action,
                        "LISTEN_CMD" : listenCmd})
        self.serveFooter()

    def serveUpload(self, qs):
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        self.serveHeader("upload")
        self.serveMotes("Upload", qs, True)
        self.serveBody("upload")
        self.serveFooter()

    lastJsonData = ""

    def serveGraphsData(self, qs):
        print "serve graphs text"
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()

        self.end_headers()

        if self.lastJsonData and not isGraphing:
            self.writeChunk(self.lastJsonData)
            self.writeFinalChunk()
            return

#        jsonData="""{
#"cols": [
#    {"id":"","label":"Timestamp","pattern":"","type":"string"},
#    {"id":"","label":"Light","pattern":"","type":"number"}
#],
#  "rows": [
#"""
#        if isGraphing:
#            for (k,v) in listenGraphData:
#                jsonData += '{"c":[{"v":"' + str(k) + '"},{"v":' + str(v) + '}]},'
#        jsonData += "]}"

#        jsonData = json.JSONEncoder().encode([['Year', 'Sales', 'Expenses'],
#          ['2004',  1000,      400],
#          ['2005',  1170,      460],
#          ['2006',  660,       1120],
#          ['2007',  1030,      540]
#        ]);

        jsonData = json.JSONEncoder().encode(graphData.getData())
        print jsonData

        self.lastJsonData = jsonData
        self.writeChunk(jsonData)
        self.writeFinalChunk()

    def serveListenData(self, qs):
        global isListening
        print "serve listen text"
        self.send_response(200)
        self.sendDefaultHeaders()
        self.end_headers()
        text = ""
        for line in listenTxt:
            text += "&nbsp;&nbsp;&nbsp;&nbsp;" + line + "<br/>"
        if text:
            self.writeChunk(text)
        self.writeFinalChunk()

    def serveJavascript(self, path, qs):
        print "serveJavascript"
        try:
            with open("javascript" + path, "r") as f:
                contents = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'application/javascript')
                self.send_header('Content-Length', str(len(contents)))
                self.end_headers()
                self.wfile.write(contents)
                f.close()
        except:
            self.serve404Error(path, qs)

    def serve404Error(self, path, qs):
        self.send_response(404)
        self.sendDefaultHeaders()
        self.end_headers()
        # TODO: write path
        self.serveHeader("404")
        self.serveFooter()


    def do_GET(self):
        o = urlparse.urlparse(self.path)
        qs = urlparse.parse_qs(o.query)

        if o.path == "/" or o.path == "/default":
            self.serveDefault()
        elif o.path == "/motes":
            self.serveMoteSelect(qs)
        elif o.path == "/config":
            self.serveConfig(qs)
        elif o.path == "/files":
            self.serveFiles(qs)
        elif o.path == "/graphs":
            self.serveGraphs(qs)
        elif o.path == "/graphs-data":
            self.serveGraphsData(qs)
        elif o.path == "/upload":
            self.serveUpload(qs)
        elif o.path == "/listen":
            self.serveListen(qs)
        elif o.path == "/listen-data":
            self.serveListenData(qs)
        elif o.path == "/test":
            self.serveTest(qs)
        elif o.path[:8] == "/jquery-":
            self.serveJavascript(o.path, qs)
        else:
            self.serve404Error(o.path, qs)

    def do_POST(self):
        # Parse the form data posted
        form = cgi.FieldStorage(
            fp = self.rfile,
            headers = self.headers,
            environ = {'REQUEST_METHOD':'POST',
                     'CONTENT_TYPE':self.headers['Content-Type'],
                     })

        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Transfer-Encoding', 'chunked')
        self.end_headers()

        file_data = None
#        for field in form.keys():
#            field_item = form[field]
#            if field_item.filename:
#                # The field contains an uploaded file
#                file_data = field_item.file.read()
#                file_len = len(file_data)
#                break

        if "code" in form.keys():
            code = form["code"].value
        else:
            code = None

        if "file" in form.keys():
            file_data = form["file"].file.read()
        else:
            file_data = None

        retcode = 0

        if file_data:
            filename = "tmp-file.ihex"
            with open(filename, "w") as outFile:
                outFile.write(file_data)
                outFile.close()

            closeAllSerial()
            for m in motes:
                r = m.tryToUpload(filename)
                if r != 0: retcode = r

        elif code:
            filename = "tmp-file.c"
            with open(filename, "w") as outFile:
                outFile.write(code)
                outFile.close()

            with open("config", "w") as outFile:
                if "config" in form.keys():
                    outFile.write(form["config"].value)
                outFile.close()

            with open("Makefile", "w") as outFile:
                outFile.write("SOURCES = tmp-file.c\n")
                outFile.write("APPMOD = App\n")
                outFile.write("PROJDIR = $(CURDIR)\n")
                outFile.write("ifndef MOSROOT\n")
                outFile.write("  MOSROOT = " + settingsInstance.getCfgValue("pathToMansOS") + "\n")
                outFile.write("endif\n")
                outFile.write("include ${MOSROOT}/mos/make/Makefile\n")
                outFile.close()

            closeAllSerial()
            for m in motes:
                r = m.tryToCompileAndUpload(filename)
                if r != 0: retcode = r

        self.serveHeader("upload")
        self.serveMotes("Upload", {}, True)
        if retcode == 0:
            self.serveBody("upload-done")
        else:
            self.serveBody("upload-error")
        self.serveFooter()


def addAllMotes():
    global motes
    # print "ports are", settingsInstance.getCfgValue("motes")
    for port in settingsInstance.getCfgValue("motes"):
        motes.append(Mote(port))

def main():
    try:
        # openAllSerial()
        port = settingsInstance.getCfgValueAsInt("port", HTTP_SERVER_PORT)
        server = BaseHTTPServer.HTTPServer(('', port), HttpServerHandler)
        addAllMotes()
        time.sleep(1)
        print("<http-server>: started, listening to TCP port {}, serial baudrate {}".format(port,
              settingsInstance.getCfgValueAsInt("baudrate", SERIAL_BAUDRATE)))
        server.serve_forever()
    except Exception, e:
        print("<http-server>: exception occurred:")
        print(e)
        return 1

if __name__ == '__main__':
    main()
