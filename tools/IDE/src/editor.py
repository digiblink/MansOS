# -*- coding: utf-8 -*-
#
# Copyright (c) 2008-2012 the MansOS team. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  * Redistributions of source code must retain the above copyright notice,
#    this list of  conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import wx.stc
import time

from edit_statement import EditStatement
from edit_condition import EditCondition
from structures import ComponentUseCase
from globals import * #@UnusedWildImport

class Editor(wx.stc.StyledTextCtrl):
    def __init__(self, parent, API):
        wx.stc.StyledTextCtrl.__init__(self, parent)
        self.API = API
        self.lastText = ''
        self.lastAutoEdit = 0
        self.lastPanel = None
        self.newMode = False

        self.last = dict()
        self.lastCount = 0

        self.SetEndAtLastLine(True)
        self.SetIndentationGuides(True)
        self.SetUseAntiAliasing(True)

        # Set style
        font = wx.SystemSettings_GetFont(wx.SYS_DEFAULT_GUI_FONT)
        self.StyleSetSpec(wx.stc.STC_STYLE_DEFAULT, "face:%s,size:10" % font.GetFaceName())
        self.StyleSetSpec(wx.stc.STC_STYLE_LINENUMBER, "back:green,face:%s,size:10" % font.GetFaceName())

        # Set captured events
        self.SetModEventMask(wx.stc.STC_PERFORMED_UNDO | wx.stc.STC_PERFORMED_REDO \
            | wx.stc.STC_MOD_DELETETEXT | wx.stc.STC_MOD_INSERTTEXT | wx.stc.STC_LEXER_START)

        self.Bind(wx.stc.EVT_STC_UPDATEUI, self.doGrammarCheck)

    def setLineNumbers(self, enable = True):
        if enable:
            width = len(str(self.GetLineCount()))
            self.SetMarginType(0, wx.stc.STC_MARGIN_NUMBER)
            self.SetMarginWidth(0, self.TextWidth(wx.stc.STC_STYLE_LINENUMBER,
                                                  (width + 1) * '0'))
        else:
            self.SetMarginWidth(0, 0)
        # Disable second margin
        self.SetMarginWidth(1, 0)

    def doGrammarCheck(self, event):
        # Mark that file has possibly changed
        self.GetParent().yieldChanges()
        self.API.clearInfoArea()

        if self.GetParent().projectType == SEAL_PROJECT:
            if self.lastText != self.GetText():
                self.lastText = self.GetText()
                self.API.sealParser.run(self.lastText)
                self.lineTracking = self.API.sealParser.lineTracking
            self.getAction(event)

    def getAction(self, event):
        # Filter unneccessary triggering(autotrigger allowed once in 1s)
        if time.time() - self.lastAutoEdit < 1:
            return

        dialog = None
        # Get statement and corresponding lines
        self.lastEdit = self.findAllStatement(self.GetCurrentLine())

        self.API.editorSplitter.Unsplit()
        if self.lastPanel != None:
            self.lastPanel.DissociateHandle()
            self.lastPanel.DestroyChildren()
            self.lastPanel.Destroy()

        if self.lastEdit[0] != None:
            if self.lastEdit[4] == STATEMENT:
                dialog = EditStatement(self.API.emptyFrame,
                        self.API, self.lastEdit, self.statementUpdateClbk)
            elif self.lastEdit[4] == CONDITION:
                dialog = EditCondition(self.API.editorSplitter,
                        self.API, self.lastEdit, self.conditionUpdateClbk)
            dialog.Hide()
        # Show or hide splash window
        if dialog != None:
            dialog.Reparent(self.API.editorSplitter)
            self.API.editorSplitter.SplitVertically(self.API.tabManager,
                                                dialog, -305)

        self.lastPanel = dialog

    def statementUpdateClbk(self, newStatement):
        if self.newMode:
            self.SetTargetStart(self.GetCurrentPos())
            self.SetTargetEnd(self.GetCurrentPos())
            self.ReplaceTarget(newStatement[0].getCode(0))
            self.lastEdit = self.findAllStatement(self.GetCurrentLine())
            self.getAction(None)
            self.newMode = False
        else:
            self.lastAutoEdit = time.time()
            self.SetTargetStart(self.PositionFromLine(newStatement[1]))
            endPos = self.PositionFromLine(newStatement[3] + 1)
            if self.GetCharAt(endPos) == '\n':
                self.SetTargetEnd(endPos - 1)
                self.ReplaceTarget(newStatement[0].getCode(0))
            else:
                self.SetTargetEnd(endPos)
                self.ReplaceTarget(newStatement[0].getCode(0) + '\n')

            self.lastEdit = self.findAllStatement(newStatement[2])

    def conditionUpdateClbk(self, name, value):
        self.lastAutoEdit = time.time()
        if self.newMode:
            self.AddText("when " + value + ":\nend")
            self.doGrammarCheck(None)
            self.lastEdit = self.findAllStatement(self.GetCurrentLine())
            self.newMode = False
        else:
            row = self.lastEdit[0].getCode(0).rstrip()
            if name == 'when':
                start = row.find("when") + len("when")
                end = row[start:].find(":")
            else:
                name = int(name)
                start = 0
                while name > 0:
                    start += row[start:].find("elsewhen") + len("elsewhen")
                    name -= 1
                end = row[start:].find(":")
            # +1 is space after keyword
            print row
            row = "{} {}{}".format(row[:start], value, row[start + end:])
            print row
            self.SetTargetStart(self.PositionFromLine(self.lastEdit[1]))
            self.SetTargetEnd(self.PositionFromLine(self.lastEdit[3]) - 1)
            self.ReplaceTarget(row)
            self.doGrammarCheck(None)
            # No need to recheck for condition, because no new lines are added
            # and no lines have been deleted.
            #self.lastEdit = self.findAllStatement(self.lastEdit[2])

    def addStatement(self):
        self.getPlaceForAdding()
        self.lastAutoEdit = time.time()
        dialog = EditStatement(self.API.editorSplitter,
                               self.API, None, self.statementUpdateClbk)
        self.splitEditor(dialog)
        self.newMode = True

    def addCondition(self):
        self.getPlaceForAdding()
        self.lastAutoEdit = time.time()
        dialog = EditCondition(self.API.editorSplitter,
                               self.API, None, self.conditionUpdateClbk)
        self.splitEditor(dialog)
        self.newMode = True

    def findAllStatement(self, lineNr):
        # Check for empty line
        line = self.GetLine(lineNr).strip()
        if line == '':
            return (None, -1, lineNr, -1)

        # Try find statement in this line
        for x in self.lineTracking["Statement"]:
            if lineNr + 1 >= x[0] and lineNr + 1 <= x[1]:
                return (x[2], x[0] - 1, lineNr, x[1] - 1, STATEMENT)

        # If no component error is throwed we dont get so far :(
        statementType, actuator, obj = self.API.getStatementType(line)

        if statementType == STATEMENT:
            return (ComponentUseCase(actuator, obj, []), lineNr,
                   lineNr, lineNr, STATEMENT)

        for x in self.lineTracking["Condition"]:
            if lineNr + 1 >= x[0] and lineNr + 1 <= x[1]:
                return (x[2], x[0] - 1, lineNr, x[1], CONDITION)

        # Try find condition in this line
        # TODO

        return (None, -1, lineNr, -1)

    def getPlaceForAdding(self):
        self.LineEndDisplay()
        self.AddText("\n\n")
        # Smtng's wrong here...
