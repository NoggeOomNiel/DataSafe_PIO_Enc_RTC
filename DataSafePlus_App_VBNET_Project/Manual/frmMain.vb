#Disable Warning IDE1006 ' Naming Styles :(

Imports System
Imports System.Data
Imports System.Drawing
Imports System.IO
Imports System.IO.Ports
Imports System.Security.Cryptography.X509Certificates
Imports System.Threading
Imports System.Windows.Forms
Imports DataSafePlus.DataSafePlus
Imports Microsoft.VisualBasic.Logging
Imports Microsoft.Win32
Imports System.Runtime.InteropServices
Imports System.Diagnostics

Partial Class frmMain
  Inherits System.Windows.Forms.Form

  'Form overrides dispose to clean up the component list.
  <System.Diagnostics.DebuggerNonUserCode()>
  Protected Overrides Sub Dispose(disposing As Boolean)
    Try
      If disposing AndAlso Me.components IsNot Nothing Then
        components.Dispose()
      End If
    Finally
      MyBase.Dispose(disposing)
    End Try
  End Sub
End Class

Public Class frmMain

  ' Serial
  Private WithEvents sp As SerialPort
  Private responseEvent As New ManualResetEvent(False)
  Private responseLock As New Object()
  Private lastResponse As String = ""
  Private serialBuffer As String = "" ' accumulator for incoming bytes
  Private autoConnectInProgress As Boolean = False
  Private autoConnectCancelled As Boolean = False
  Private autoConnectThread As Thread = Nothing
  Private Const EXPECTED_RESPONSE As String = "PICO_READY" ' "DataSafe"
  Private Const IDENTIFY_TIMEOUT As Integer = 1500 ' ms
  Private Const SelectedBaudRate As Integer = 115200

  Private timestampCaptured As Boolean = False
  Private timestampValue As Long = 0
  Private timeDifference As Long

  ' Captured timestamp from identification sequence (set when IdentifyDeviceOnPort detects it)
  Private lastIdentifiedUnixTimestamp As Long = 0

  Private portUsed As String = ""

  'Delegates
  Private Delegate Sub LogInvoker(text As String)

  'Instatiate dataset
  Private ds As New dsDataSafe

  ' Password reveal state for DataGridView password column
  Private passwordRevealActive As Boolean = False
  Private passwordRevealRow As Integer = -1
  Private passwordRevealCol As Integer = -1

  Private deviceConnected As Boolean = False


  ' --- P/Invoke helpers for foreground window handling ---
  <DllImport("user32.dll", SetLastError:=True)>
  Private Shared Function SetForegroundWindow(hWnd As IntPtr) As Boolean
  End Function

  <DllImport("user32.dll", SetLastError:=True)>
  Private Shared Function GetForegroundWindow() As IntPtr
  End Function

  <DllImport("user32.dll", SetLastError:=True)>
  Private Shared Function GetWindowThreadProcessId(hWnd As IntPtr, ByRef lpdwProcessId As Integer) As Integer
  End Function

  <DllImport("kernel32.dll")>
  Private Shared Function GetCurrentThreadId() As Integer
  End Function

  <DllImport("user32.dll", SetLastError:=True)>
  Private Shared Function AttachThreadInput(idAttach As Integer, idAttachTo As Integer, fAttach As Boolean) As Boolean
  End Function

  <DllImport("user32.dll", SetLastError:=True)>
  Private Shared Function ShowWindow(hWnd As IntPtr, nCmdShow As Integer) As Boolean
  End Function

  Private Const SW_SHOW As Integer = 5

  'Required by the Windows Form Designer
  Private components As System.ComponentModel.IContainer

  Friend WithEvents dgvDataSafeDataGrid As DataGridView
  Friend WithEvents RawDataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents NameDataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents WebsiteDataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents Action1DataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents UserNameDataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents PasswordDataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents Action2DataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents RankingDataGridViewTextBoxColumn As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn1 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn2 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn3 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn4 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn5 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn6 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn7 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn8 As DataGridViewTextBoxColumn
  Friend WithEvents lblPin As Label
  Friend WithEvents mtbPin As MaskedTextBox
  Friend WithEvents btnLoad As Button
  Friend WithEvents DataGridViewTextBoxColumn9 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn10 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn11 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn12 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn13 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn14 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn15 As DataGridViewTextBoxColumn
  Friend WithEvents DataGridViewTextBoxColumn16 As DataGridViewTextBoxColumn
  Friend WithEvents StatusStrip1 As StatusStrip
  Friend WithEvents statlabel As ToolStripStatusLabel
  Friend WithEvents btnSend As Button
  Friend WithEvents scMain As SplitContainer
  Friend WithEvents btnRebootPico As Button
  Friend WithEvents btnLoadCSV As Button
  Friend WithEvents btnResync As Button
  Friend WithEvents tmrCheckComPort As System.Windows.Forms.Timer
  Friend WithEvents btnSaveCSV As Button
  Friend WithEvents Raw As DataGridViewTextBoxColumn
  Friend WithEvents _Name As DataGridViewTextBoxColumn
  Friend WithEvents Website As DataGridViewTextBoxColumn
  Friend WithEvents Action1 As DataGridViewTextBoxColumn
  Friend WithEvents UserName As DataGridViewTextBoxColumn
  Friend WithEvents Password As DataGridViewTextBoxColumn
  Friend WithEvents Action2 As DataGridViewTextBoxColumn
  Friend WithEvents Ranking As DataGridViewTextBoxColumn
  Friend WithEvents ToolTip1 As ToolTip
  Friend WithEvents txtSearch As TextBox
  Friend WithEvents pbSearch As PictureBox

  <System.Diagnostics.DebuggerStepThrough()>
  Private Sub InitializeComponent()
    components = New ComponentModel.Container()
    Dim DataGridViewCellStyle1 As DataGridViewCellStyle = New DataGridViewCellStyle()
    Dim DataGridViewCellStyle2 As DataGridViewCellStyle = New DataGridViewCellStyle()
    Dim DataGridViewCellStyle3 As DataGridViewCellStyle = New DataGridViewCellStyle()
    Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmMain))
    dgvDataSafeDataGrid = New DataGridView()
    Raw = New DataGridViewTextBoxColumn()
    _Name = New DataGridViewTextBoxColumn()
    Website = New DataGridViewTextBoxColumn()
    Action1 = New DataGridViewTextBoxColumn()
    UserName = New DataGridViewTextBoxColumn()
    Password = New DataGridViewTextBoxColumn()
    Action2 = New DataGridViewTextBoxColumn()
    Ranking = New DataGridViewTextBoxColumn()
    lblPin = New Label()
    mtbPin = New MaskedTextBox()
    btnLoad = New Button()
    StatusStrip1 = New StatusStrip()
    statlabel = New ToolStripStatusLabel()
    btnSend = New Button()
    scMain = New SplitContainer()
    pbSearch = New PictureBox()
    txtSearch = New TextBox()
    btnSaveCSV = New Button()
    btnResync = New Button()
    btnLoadCSV = New Button()
    btnRebootPico = New Button()
    tmrCheckComPort = New System.Windows.Forms.Timer(components)
    ToolTip1 = New ToolTip(components)
    CType(dgvDataSafeDataGrid, ComponentModel.ISupportInitialize).BeginInit()
    StatusStrip1.SuspendLayout()
    CType(scMain, ComponentModel.ISupportInitialize).BeginInit()
    scMain.Panel1.SuspendLayout()
    scMain.Panel2.SuspendLayout()
    scMain.SuspendLayout()
    CType(pbSearch, ComponentModel.ISupportInitialize).BeginInit()
    SuspendLayout()
    ' 
    ' dgvDataSafeDataGrid
    ' 
    dgvDataSafeDataGrid.AllowUserToResizeColumns = False
    dgvDataSafeDataGrid.AllowUserToResizeRows = False
    DataGridViewCellStyle1.BackColor = Color.FromArgb(CByte(64), CByte(64), CByte(64))
    dgvDataSafeDataGrid.AlternatingRowsDefaultCellStyle = DataGridViewCellStyle1
    dgvDataSafeDataGrid.BackgroundColor = SystemColors.Control
    DataGridViewCellStyle2.Alignment = DataGridViewContentAlignment.MiddleCenter
    DataGridViewCellStyle2.BackColor = SystemColors.Control
    DataGridViewCellStyle2.Font = New Font("Segoe UI", 9.0F)
    DataGridViewCellStyle2.ForeColor = SystemColors.WindowText
    DataGridViewCellStyle2.SelectionBackColor = SystemColors.Highlight
    DataGridViewCellStyle2.SelectionForeColor = SystemColors.HighlightText
    DataGridViewCellStyle2.WrapMode = DataGridViewTriState.True
    dgvDataSafeDataGrid.ColumnHeadersDefaultCellStyle = DataGridViewCellStyle2
    dgvDataSafeDataGrid.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.AutoSize
    dgvDataSafeDataGrid.Columns.AddRange(New DataGridViewColumn() {Raw, _Name, Website, Action1, UserName, Password, Action2, Ranking})
    dgvDataSafeDataGrid.Dock = DockStyle.Fill
    dgvDataSafeDataGrid.Location = New Point(0, 0)
    dgvDataSafeDataGrid.Margin = New Padding(5)
    dgvDataSafeDataGrid.Name = "dgvDataSafeDataGrid"
    DataGridViewCellStyle3.Alignment = DataGridViewContentAlignment.MiddleLeft
    DataGridViewCellStyle3.BackColor = Color.Silver
    DataGridViewCellStyle3.Font = New Font("Segoe UI", 9.0F)
    DataGridViewCellStyle3.ForeColor = SystemColors.WindowText
    DataGridViewCellStyle3.SelectionBackColor = SystemColors.Highlight
    DataGridViewCellStyle3.SelectionForeColor = SystemColors.HighlightText
    DataGridViewCellStyle3.WrapMode = DataGridViewTriState.True
    dgvDataSafeDataGrid.RowHeadersDefaultCellStyle = DataGridViewCellStyle3
    dgvDataSafeDataGrid.RowHeadersWidth = 20
    dgvDataSafeDataGrid.Size = New Size(880, 472)
    dgvDataSafeDataGrid.TabIndex = 0
    ' 
    ' Raw
    ' 
    Raw.HeaderText = "Raw"
    Raw.Name = "Raw"
    Raw.SortMode = DataGridViewColumnSortMode.NotSortable
    Raw.Visible = False
    ' 
    ' _Name
    ' 
    _Name.AutoSizeMode = DataGridViewAutoSizeColumnMode.None
    _Name.DataPropertyName = "_Name"
    _Name.HeaderText = "Name"
    _Name.Name = "_Name"
    _Name.ToolTipText = "Sorting will reset usage count to 0"
    _Name.Width = 150
    ' 
    ' Website
    ' 
    Website.AutoSizeMode = DataGridViewAutoSizeColumnMode.None
    Website.DataPropertyName = "Website"
    Website.HeaderText = "Website"
    Website.Name = "Website"
    Website.SortMode = DataGridViewColumnSortMode.NotSortable
    Website.Width = 200
    ' 
    ' Action1
    ' 
    Action1.AutoSizeMode = DataGridViewAutoSizeColumnMode.None
    Action1.DataPropertyName = "Action1"
    Action1.HeaderText = "Login"
    Action1.Name = "Action1"
    Action1.SortMode = DataGridViewColumnSortMode.NotSortable
    Action1.Width = 60
    ' 
    ' UserName
    ' 
    UserName.AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
    UserName.DataPropertyName = "Username"
    UserName.HeaderText = "Username"
    UserName.Name = "UserName"
    UserName.SortMode = DataGridViewColumnSortMode.NotSortable
    ' 
    ' Password
    ' 
    Password.DataPropertyName = "Password"
    Password.HeaderText = "Password"
    Password.Name = "Password"
    Password.SortMode = DataGridViewColumnSortMode.NotSortable
    Password.Width = 150
    ' 
    ' Action2
    ' 
    Action2.AutoSizeMode = DataGridViewAutoSizeColumnMode.None
    Action2.DataPropertyName = "Action2"
    Action2.HeaderText = "Return"
    Action2.Name = "Action2"
    Action2.SortMode = DataGridViewColumnSortMode.NotSortable
    Action2.Width = 60
    ' 
    ' Ranking
    ' 
    Ranking.DataPropertyName = "Ranking"
    Ranking.HeaderText = "Order"
    Ranking.Name = "Ranking"
    Ranking.SortMode = DataGridViewColumnSortMode.NotSortable
    Ranking.Visible = False
    Ranking.Width = 20
    ' 
    ' lblPin
    ' 
    lblPin.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    lblPin.Location = New Point(5, 11)
    lblPin.Name = "lblPin"
    lblPin.Size = New Size(72, 19)
    lblPin.TabIndex = 1
    lblPin.Text = "Enter Pin:"
    lblPin.TextAlign = ContentAlignment.MiddleCenter
    ' 
    ' mtbPin
    ' 
    mtbPin.BorderStyle = BorderStyle.FixedSingle
    mtbPin.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    mtbPin.Location = New Point(78, 9)
    mtbPin.Mask = "0000"
    mtbPin.Name = "mtbPin"
    mtbPin.Size = New Size(47, 23)
    mtbPin.TabIndex = 2
    mtbPin.TextAlign = HorizontalAlignment.Center
    ' 
    ' btnLoad
    ' 
    btnLoad.Enabled = False
    btnLoad.FlatStyle = FlatStyle.System
    btnLoad.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    btnLoad.Location = New Point(192, 17)
    btnLoad.Name = "btnLoad"
    btnLoad.Size = New Size(140, 35)
    btnLoad.TabIndex = 3
    btnLoad.Text = "Load DataSafe Items"
    btnLoad.UseVisualStyleBackColor = True
    ' 
    ' StatusStrip1
    ' 
    StatusStrip1.Items.AddRange(New ToolStripItem() {statlabel})
    StatusStrip1.Location = New Point(0, 539)
    StatusStrip1.Name = "StatusStrip1"
    StatusStrip1.Size = New Size(880, 22)
    StatusStrip1.SizingGrip = False
    StatusStrip1.TabIndex = 4
    StatusStrip1.Text = "StatusStrip1"
    ' 
    ' statlabel
    ' 
    statlabel.Name = "statlabel"
    statlabel.Size = New Size(35, 17)
    statlabel.Text = "Idle..."
    ' 
    ' btnSend
    ' 
    btnSend.Enabled = False
    btnSend.FlatStyle = FlatStyle.System
    btnSend.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    btnSend.Location = New Point(338, 17)
    btnSend.Name = "btnSend"
    btnSend.Size = New Size(140, 35)
    btnSend.TabIndex = 5
    btnSend.Text = "Update DataSafe Items"
    btnSend.UseVisualStyleBackColor = True
    ' 
    ' scMain
    ' 
    scMain.Dock = DockStyle.Fill
    scMain.Location = New Point(0, 0)
    scMain.Name = "scMain"
    scMain.Orientation = Orientation.Horizontal
    ' 
    ' scMain.Panel1
    ' 
    scMain.Panel1.Controls.Add(pbSearch)
    scMain.Panel1.Controls.Add(txtSearch)
    scMain.Panel1.Controls.Add(btnSaveCSV)
    scMain.Panel1.Controls.Add(btnResync)
    scMain.Panel1.Controls.Add(btnLoadCSV)
    scMain.Panel1.Controls.Add(btnRebootPico)
    scMain.Panel1.Controls.Add(lblPin)
    scMain.Panel1.Controls.Add(btnSend)
    scMain.Panel1.Controls.Add(mtbPin)
    scMain.Panel1.Controls.Add(btnLoad)
    ' 
    ' scMain.Panel2
    ' 
    scMain.Panel2.Controls.Add(dgvDataSafeDataGrid)
    scMain.Size = New Size(880, 539)
    scMain.SplitterDistance = 63
    scMain.TabIndex = 6
    ' 
    ' pbSearch
    ' 
    pbSearch.BackColor = Color.White
    pbSearch.Image = CType(resources.GetObject("pbSearch.Image"), Image)
    pbSearch.Location = New Point(147, 37)
    pbSearch.Name = "pbSearch"
    pbSearch.Size = New Size(35, 23)
    pbSearch.SizeMode = PictureBoxSizeMode.StretchImage
    pbSearch.TabIndex = 11
    pbSearch.TabStop = False
    ' 
    ' txtSearch
    ' 
    txtSearch.BorderStyle = BorderStyle.FixedSingle
    txtSearch.Location = New Point(9, 37)
    txtSearch.Name = "txtSearch"
    txtSearch.Size = New Size(138, 23)
    txtSearch.TabIndex = 10
    txtSearch.TextAlign = HorizontalAlignment.Center
    ' 
    ' btnSaveCSV
    ' 
    btnSaveCSV.Enabled = False
    btnSaveCSV.FlatStyle = FlatStyle.System
    btnSaveCSV.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    btnSaveCSV.Location = New Point(586, 17)
    btnSaveCSV.Name = "btnSaveCSV"
    btnSaveCSV.Size = New Size(96, 35)
    btnSaveCSV.TabIndex = 9
    btnSaveCSV.Text = "Save CSV File"
    btnSaveCSV.UseVisualStyleBackColor = True
    ' 
    ' btnResync
    ' 
    btnResync.Enabled = False
    btnResync.FlatStyle = FlatStyle.System
    btnResync.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    btnResync.Location = New Point(687, 17)
    btnResync.Name = "btnResync"
    btnResync.Size = New Size(89, 35)
    btnResync.TabIndex = 8
    btnResync.Text = "Update Time on DataSafe"
    btnResync.UseVisualStyleBackColor = True
    ' 
    ' btnLoadCSV
    ' 
    btnLoadCSV.Enabled = False
    btnLoadCSV.FlatStyle = FlatStyle.System
    btnLoadCSV.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    btnLoadCSV.Location = New Point(484, 17)
    btnLoadCSV.Name = "btnLoadCSV"
    btnLoadCSV.Size = New Size(96, 35)
    btnLoadCSV.TabIndex = 7
    btnLoadCSV.Text = "Load CSV File"
    btnLoadCSV.UseVisualStyleBackColor = True
    ' 
    ' btnRebootPico
    ' 
    btnRebootPico.Enabled = False
    btnRebootPico.FlatStyle = FlatStyle.System
    btnRebootPico.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
    btnRebootPico.Location = New Point(782, 17)
    btnRebootPico.Name = "btnRebootPico"
    btnRebootPico.Size = New Size(89, 35)
    btnRebootPico.TabIndex = 6
    btnRebootPico.Text = "Reboot DataSafe"
    btnRebootPico.UseVisualStyleBackColor = True
    ' 
    ' tmrCheckComPort
    ' 
    tmrCheckComPort.Interval = 1000
    ' 
    ' frmMain
    ' 
    AutoScaleDimensions = New SizeF(7.0F, 15.0F)
    AutoScaleMode = AutoScaleMode.Font
    ClientSize = New Size(880, 561)
    Controls.Add(scMain)
    Controls.Add(StatusStrip1)
    FormBorderStyle = FormBorderStyle.FixedSingle
    Icon = CType(resources.GetObject("$this.Icon"), Icon)
    MaximizeBox = False
    Name = "frmMain"
    SizeGripStyle = SizeGripStyle.Hide
    Text = "DataSafePlus Utility  (Not Connected)"
    CType(dgvDataSafeDataGrid, ComponentModel.ISupportInitialize).EndInit()
    StatusStrip1.ResumeLayout(False)
    StatusStrip1.PerformLayout()
    scMain.Panel1.ResumeLayout(False)
    scMain.Panel1.PerformLayout()
    scMain.Panel2.ResumeLayout(False)
    CType(scMain, ComponentModel.ISupportInitialize).EndInit()
    scMain.ResumeLayout(False)
    CType(pbSearch, ComponentModel.ISupportInitialize).EndInit()
    ResumeLayout(False)
    PerformLayout()
  End Sub








  ' Known browser process names (lowercase); extend if needed
  Private ReadOnly BrowserProcessNames As String() = New String() {"msedge", "chrome", "firefox", "iexplore", "brave", "opera"}

  Private Function IsProcessNameBrowser(procName As String) As Boolean
    If String.IsNullOrEmpty(procName) Then Return False
    For Each bn In BrowserProcessNames
      If String.Equals(procName, bn, StringComparison.OrdinalIgnoreCase) Then
        Return True
      End If
    Next
    Return False
  End Function

  Private Function ForegroundWindowIsBrowser() As IntPtr
    Try
      Dim fg = GetForegroundWindow()
      If fg = IntPtr.Zero Then Return IntPtr.Zero
      Dim pid As Integer = 0
      GetWindowThreadProcessId(fg, pid)
      If pid = 0 Then Return IntPtr.Zero
      Try
        Dim p = Process.GetProcessById(pid)
        If p IsNot Nothing AndAlso IsProcessNameBrowser(p.ProcessName) Then
          Return fg
        End If
      Catch
        ' ignore
      End Try
    Catch
    End Try
    Return IntPtr.Zero
  End Function

  Private Function FindAnyBrowserWindowHandle() As IntPtr
    ' Prefer foreground browser if present
    Dim fgBrowser = ForegroundWindowIsBrowser()
    If fgBrowser <> IntPtr.Zero Then
      Return fgBrowser
    End If

    ' Otherwise search processes for a browser main window
    For Each item In BrowserProcessNames
      Try
        Dim procs = Process.GetProcessesByName(item)
        For Each p In procs
          Try
            If p.MainWindowHandle <> IntPtr.Zero Then
              Return p.MainWindowHandle
            End If
          Catch
          End Try
        Next
      Catch
      End Try
    Next

    ' No browser window found
    Return IntPtr.Zero
  End Function

  Private Function ForceSetForegroundWindow(target As IntPtr) As Boolean
    Try
      If target = IntPtr.Zero Then Return False
      Dim fg = GetForegroundWindow()
      If fg = target Then Return True

      ' Try to attach input threads to allow SetForegroundWindow in many cases
      Dim curThread = GetCurrentThreadId()
      Dim fgThread = GetWindowThreadProcessId(fg, 0)
      Dim attached As Boolean = False
      Try
        attached = AttachThreadInput(curThread, fgThread, True)
      Catch
        attached = False
      End Try

      ' Restore if minimized
      Try
        ShowWindow(target, SW_SHOW)
      Catch
      End Try

      Dim result As Boolean = False
      Try
        result = SetForegroundWindow(target)
      Catch
        result = False
      End Try

      If attached Then
        Try
          AttachThreadInput(curThread, fgThread, False)
        Catch
        End Try
      End If

      Return result
    Catch
      Return False
    End Try
  End Function

  Private Sub SendTextToBrowserClipboardPaste(text As String)
    If String.IsNullOrEmpty(text) Then Return

    ' All clipboard operations and SendKeys require STA.
    Dim t As New Thread(Sub()
                          Try
                            Dim origData As IDataObject = Nothing
                            Try
                              origData = Clipboard.GetDataObject()
                            Catch
                              origData = Nothing
                            End Try

                            ' Determine target browser window: prefer the foreground browser (active tab) if present,
                            ' otherwise pick the first running browser window found.
                            Dim targetHwnd As IntPtr = ForegroundWindowIsBrowser()
                            If targetHwnd = IntPtr.Zero Then
                              targetHwnd = FindAnyBrowserWindowHandle()
                            End If

                            If targetHwnd = IntPtr.Zero Then
                              Me.BeginInvoke(Sub() Log("No browser window found to paste into."))
                              Return
                            End If

                            ' Put text onto clipboard
                            Try
                              Clipboard.Clear()
                              Clipboard.SetText(text)
                            Catch ex As Exception
                              Me.BeginInvoke(Sub() Log("Clipboard set failed: " & ex.Message))
                              Return
                            End Try

                            ' If target is not currently foreground, attempt to bring it forward.
                            Dim fg = GetForegroundWindow()
                            If fg <> targetHwnd Then
                              ForceSetForegroundWindow(targetHwnd)
                              Thread.Sleep(120)
                            Else
                              ' Give small delay to ensure caret is ready
                              Thread.Sleep(80)
                            End If

                            ' Paste via Ctrl+V into active tab/field
                            Try
                              SendKeys.SendWait("^{v}")
                              SendKeys.SendWait(vbTab)
                            Catch ex As Exception
                              Me.BeginInvoke(Sub() Log("SendKeys failed: " & ex.Message))
                            End Try

                            Thread.Sleep(60)

                            ' Restore original clipboard if we captured it
                            If origData IsNot Nothing Then
                              Try
                                Clipboard.SetDataObject(origData, True)
                              Catch
                                ' ignore
                              End Try
                            End If

                            Me.BeginInvoke(Sub() Log("Pasted cell text to browser."))
                          Catch ex As Exception
                            Me.BeginInvoke(Sub() Log("SendTextToBrowser error: " & ex.Message))
                          End Try
                        End Sub)
    t.SetApartmentState(ApartmentState.STA)
    t.IsBackground = True
    t.Start()
  End Sub
  ' --- end P/Invoke / browser helpers ---


  Public Sub New()

    InitializeComponent()
    ' Apply initial theme based on Windows setting
    ApplyWindowsTheme()

    'Bind to datagrid
    dgvDataSafeDataGrid.AutoGenerateColumns = False
    dgvDataSafeDataGrid.DataSource = ds.dtAllitems
    dgvDataSafeDataGrid.Columns("Raw").Visible = False

  End Sub

  Private Sub AutoConnectToDataSafe()
    If autoConnectInProgress Then
      ' Cancel auto-connect if in progress
      autoConnectCancelled = True
      Me.Text = "DataSafePlus (Connecting to DataSafe...)"
      If autoConnectThread IsNot Nothing AndAlso autoConnectThread.IsAlive Then
        Try
          autoConnectThread.Join(500) ' wait briefly for thread to exit
        Catch
        End Try
        autoConnectThread = Nothing
      End If
      autoConnectInProgress = False
      Return
    End If

    autoConnectInProgress = True
    autoConnectCancelled = False

    ' Run auto-connect in background to avoid UI freezing
    autoConnectThread = New Thread(AddressOf AutoConnectToDevice) With {.IsBackground = True}
    autoConnectThread.Start()

  End Sub

  Private Sub resyncPico()

    If sp Is Nothing OrElse Not sp.IsOpen Then
      statlabel.Text = "Not connected to DataSafe. Cannot reboot."
      Return
    End If

    ' Disable reboot button briefly
    Try
      btnResync.Enabled = False
    Catch
    End Try

    ' Capture pin on UI thread to avoid cross-thread access
    Dim pin As String = mtbPin.Text.Trim()

    ' Perform resync sequence on a background thread so UI stays responsive
    Dim resyncThread As New Thread(Sub()
                                     Try
                                       If sp Is Nothing OrElse Not sp.IsOpen Then
                                         LogFromThread("Cannot RESYNC: not connected to device.")
                                         Return
                                       End If

                                       ' Prepare to capture responses
                                       responseEvent.Reset()
                                       SyncLock responseLock
                                         lastResponse = ""
                                       End SyncLock

                                       ' 1) Send RESYNC
                                       Try
                                         sp.WriteLine($"RESYNC,{pin}")
                                         LogFromThread("> RESYNC")
                                       Catch ex As Exception
                                         LogFromThread("RESYNC send error: " & ex.Message)
                                         Return
                                       End Try

                                       ' 2) Wait for PICO_READY
                                       Dim sw As Stopwatch = Stopwatch.StartNew()
                                       Dim gotReady As Boolean = False
                                       While sw.ElapsedMilliseconds < 5000
                                         If responseEvent.WaitOne(200) Then
                                           responseEvent.Reset()
                                           Dim resp As String = ""
                                           SyncLock responseLock
                                             resp = lastResponse
                                           End SyncLock
                                           If Not String.IsNullOrEmpty(resp) Then
                                             LogFromThread($"< {resp}")
                                             If resp.Contains(EXPECTED_RESPONSE) Then
                                               gotReady = True
                                               Exit While
                                             End If
                                           End If
                                         End If
                                       End While

                                       If Not gotReady Then
                                         LogFromThread("RESYNC failed: no PICO_READY received.")
                                         Return
                                       End If

                                       ' 3) Send HOST_ACK
                                       Try
                                         sp.WriteLine("HOST_ACK")
                                         LogFromThread("> HOST_ACK")
                                       Catch ex As Exception
                                         LogFromThread("HOST_ACK send error: " & ex.Message)
                                         Return
                                       End Try

                                       ' 4) Wait for SYNC_TIME_REQUEST
                                       sw.Restart()
                                       Dim gotSyncRequest As Boolean = False
                                       While sw.ElapsedMilliseconds < 5000
                                         If responseEvent.WaitOne(200) Then
                                           responseEvent.Reset()
                                           Dim resp As String = ""
                                           SyncLock responseLock
                                             resp = lastResponse
                                           End SyncLock
                                           If Not String.IsNullOrEmpty(resp) Then
                                             LogFromThread($"< {resp}")
                                             If resp.Contains("SYNC_TIME_REQUEST") Then
                                               gotSyncRequest = True
                                               Exit While
                                             End If
                                           End If
                                         End If
                                       End While

                                       If Not gotSyncRequest Then
                                         LogFromThread("RESYNC failed: no SYNC_TIME_REQUEST received.")
                                         Return
                                       End If

                                       ' 5) Send captured timestampValue
                                       Try
                                         Dim unixTime = CLng((DateTime.UtcNow - New DateTime(1970, 1, 1)).TotalSeconds)
                                         sp.WriteLine(unixTime)
                                         LogFromThread($"> {timestampValue}")
                                         Me.Invoke(Sub() statlabel.Text = "Time sync sent.")
                                       Catch ex As Exception
                                         LogFromThread("Send timestamp error: " & ex.Message)
                                       End Try

                                     Catch ex As Exception
                                       LogFromThread("Resync sequence error: " & ex.Message)
                                     End Try
                                   End Sub) With {.IsBackground = True}
    resyncThread.Start()

    Try
      btnResync.Enabled = True
    Catch
    End Try

  End Sub

  Private Sub AutoConnectToDevice()
    statlabel.Text = "Auto-connecting to DataSafe..."

    Dim availablePorts = SerialPort.GetPortNames()
    While availablePorts.Length = 0
      If autoConnectCancelled Then
        statlabel.Text = "Auto-connect cancelled."
        AutoConnectComplete(False)
        deviceConnected = False
        Return
      End If
      statlabel.Text = "No COM ports found. Retrying..."
      Thread.Sleep(2000)
      availablePorts = SerialPort.GetPortNames()
    End While

    While autoConnectInProgress
      statlabel.Text = $"Found {availablePorts.Length} COM port(s): {String.Join(", ", availablePorts)}"

      For Each portName In availablePorts
        If autoConnectCancelled Then
          statlabel.Text = "Auto-connect cancelled."
          AutoConnectComplete(False)
          deviceConnected = False
          Return
        End If

        statlabel.Text = $"Testing port {portName}..."

        ' Try to identify device on this port
        Dim identified = IdentifyDeviceOnPort(portName)

        If identified Then
          statlabel.Text = $"DataSafe device found on {portName}!"
          Thread.Sleep(1000)
          ' Update UI on main thread and connect to this port
          Me.Invoke(Sub()
                      ' Connect to the port
                      ConnectToPort(portName)
                    End Sub)
          AutoConnectComplete(True)
          deviceConnected = True

          Return
        Else
          statlabel.Text = $"No DataSafe device on {portName}."
        End If
      Next

      statlabel.Text = "No DataSafe device found on any COM port. Retrying in 5 seconds..."
      Thread.Sleep(5000) ' Wait before retrying
      availablePorts = SerialPort.GetPortNames()
    End While
  End Sub

  Private Sub AutoConnectComplete(success As Boolean)
    If Me.InvokeRequired Then
      Me.Invoke(Sub() AutoConnectComplete(success))
      Return
    End If

    autoConnectInProgress = False
    If success Then
      statlabel.Text = "Auto-connect successful."
      Me.Text = "DataSafePlus (Connected to: " & sp.PortName & ")"
      deviceConnected = True
      Thread.Sleep(1000)

    Else
      If Not autoConnectCancelled Then
        statlabel.Text = "Auto-connect failed. No DataSafe device found."
      End If
    End If
  End Sub

  Private Function IdentifyDeviceOnPort(portName As String) As Boolean
    Dim tempSerialPort As SerialPort = Nothing

    Try
      ' Create temporary serial port for identification
      tempSerialPort = New SerialPort(portName, SelectedBaudRate, Parity.None, 8, StopBits.One) With {
          .Encoding = System.Text.Encoding.UTF8,
          .NewLine = vbLf,
          .ReadTimeout = IDENTIFY_TIMEOUT,
          .WriteTimeout = IDENTIFY_TIMEOUT,
          .DtrEnable = True,
          .RtsEnable = True
      }
      tempSerialPort.Open()
      tempSerialPort.DiscardInBuffer()
      tempSerialPort.DiscardOutBuffer()

      ' Set up event handler for response
      Dim responseBuffer As String = ""
      Dim responseEventLocal As New ManualResetEvent(False)
      Dim responseLockLocal As New Object()

      Dim dataReceivedHandler As SerialDataReceivedEventHandler =
          Sub(sender As Object, e As SerialDataReceivedEventArgs)
            Try
              If tempSerialPort.BytesToRead > 0 Then
                Dim data = tempSerialPort.ReadExisting()
                SyncLock responseLockLocal
                  responseBuffer &= data
                End SyncLock
                responseEventLocal.Set()
              End If
            Catch ex As Exception
              ' Ignore errors during identification
            End Try
          End Sub

      AddHandler tempSerialPort.DataReceived, dataReceivedHandler

      Thread.Sleep(300) ' Small delay to ensure command is sent

      ' Wait for response with timeout
      If responseEventLocal.WaitOne(IDENTIFY_TIMEOUT) Then
        SyncLock responseLockLocal
          ' Normalize and split into lines (device uses vbLf)
          Dim lines = responseBuffer.Split(New String() {vbLf}, StringSplitOptions.RemoveEmptyEntries)
          ' Trim each line
          For i As Integer = 0 To lines.Length - 1
            lines(i) = lines(i).Trim()
          Next

          ' Find the line that contains EXPECTED_RESPONSE
          For i As Integer = 0 To lines.Length - 1
            If lines(i) IsNot Nothing AndAlso lines(i).Contains(EXPECTED_RESPONSE) Then
              ' Check if the next line is already present and parse it as unix timestamp
              If i + 1 < lines.Length Then
                Dim candidate = lines(i + 1)
                Dim parsed As Long = 0
                If Long.TryParse(candidate, parsed) Then
                  timestampCaptured = True
                  timestampValue = parsed
                End If
              Else
                ' Next line not yet received; wait briefly for the next chunk (up to IDENTIFY_TIMEOUT)
                ' Release lock while waiting for more data
                ' Wait up to half the IDENTIFY_TIMEOUT for the timestamp line
                Monitor.Exit(responseLockLocal)
                Try
                  If responseEventLocal.WaitOne(Math.Max(200, IDENTIFY_TIMEOUT \ 2)) Then
                    SyncLock responseLockLocal
                      Dim newLines = responseBuffer.Split(New String() {vbLf}, StringSplitOptions.RemoveEmptyEntries)
                      For j As Integer = 0 To newLines.Length - 1
                        newLines(j) = newLines(j).Trim()
                      Next
                      ' re-find the EXPECTED_RESPONSE index and check following line
                      For j As Integer = 0 To newLines.Length - 1
                        If newLines(j) IsNot Nothing AndAlso newLines(j).Contains(EXPECTED_RESPONSE) Then
                          If j + 1 < newLines.Length Then
                            Dim candidate2 = newLines(j + 1)
                            Dim parsed2 As Long = 0
                            If Long.TryParse(candidate2, parsed2) Then
                              timestampCaptured = True
                              timestampValue = parsed2
                              Exit For
                            End If
                          End If
                        End If
                      Next
                    End SyncLock
                  End If
                Finally
                  ' Ensure the lock is held again for subsequent code (if not already)
                  If Not Monitor.IsEntered(responseLockLocal) Then
                    SyncLock responseLockLocal
                      ' no-op to re-enter the lock
                    End SyncLock
                  End If
                End Try
              End If

              If timestampCaptured Then
                ' Store the timestamp on the form for later use and log it
                lastIdentifiedUnixTimestamp = timestampValue
                timeDifference = Math.Abs(getUnixDelta(lastIdentifiedUnixTimestamp))
                LogFromThread($"Identified device on {portName} with timestamp {timestampValue}")
              Else
                LogFromThread($"Identified EXPECTED_RESPONSE on {portName} but no timestamp parsed yet.")
              End If

              ' Clean up and return success
              RemoveHandler tempSerialPort.DataReceived, dataReceivedHandler
              tempSerialPort.Close()
              tempSerialPort.Dispose()
              Return True
            End If
          Next
        End SyncLock
      End If

      ' Clean up event handler
      RemoveHandler tempSerialPort.DataReceived, dataReceivedHandler
      With tempSerialPort
        .DiscardInBuffer()
        .DiscardOutBuffer()
        .Close()
        .Dispose()
      End With

    Catch ex As Exception
      'Do nothing - this is normal during auto-detection

    Finally
      ' Always close the temporary port
      If tempSerialPort IsNot Nothing AndAlso tempSerialPort.IsOpen Then
        Try
          tempSerialPort.Close()
          tempSerialPort.Dispose()
        Catch
          'Do nothing
        End Try
      End If
    End Try

    Return False
  End Function

  Public Shared Function getUnixDelta(_timestampValue As Long) As Double
    ' Get current system time as Unix timestamp (seconds since 1970-01-01 UTC)
    Dim currentUnix As Long = CLng(
        (DateTime.Now - New DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Local)).TotalSeconds
    )

    ' Difference in seconds
    Dim diffSeconds As Long = currentUnix - _timestampValue

    ' Convert to minutes (can be negative if timestampValue is in the future)
    Return diffSeconds / 60.0
  End Function

  Private Sub ConnectToPort(portName As String)
    If sp IsNot Nothing AndAlso sp.IsOpen Then
      Try
        sp.Close()
        RemoveHandler sp.DataReceived, AddressOf Sp_DataReceived
      Catch
      End Try
    End If

    sp = New SerialPort(portName, SelectedBaudRate, Parity.None, 8, StopBits.One) With {
        .Encoding = System.Text.Encoding.UTF8,
        .NewLine = vbLf,
        .ReadTimeout = 2000,
        .WriteTimeout = 2000,
        .DtrEnable = True,
        .RtsEnable = True
    }

    Try
      sp.Open()
      sp.DiscardInBuffer()
      sp.DiscardOutBuffer()
      AddHandler sp.DataReceived, AddressOf Sp_DataReceived
      Log("Connected to " & sp.PortName)
      Thread.Sleep(1000)

      ' If an auto-connect thread was running, cancel it now that we have a successful connection.
      If autoConnectInProgress Then
        autoConnectCancelled = True
        autoConnectInProgress = False
        Log("Successful connection.")
        Thread.Sleep(1000)
        If autoConnectThread IsNot Nothing AndAlso autoConnectThread.IsAlive Then
          Try
            autoConnectThread.Join(500)
          Catch
          End Try
          autoConnectThread = Nothing
        End If

        ' Start monitoring COM port status
        portUsed = portName
        tmrCheckComPort.Enabled = True
        tmrCheckComPort.Start()

      End If

    Catch ex As Exception
      Log("Open error: " & ex.Message)
      sp = Nothing
    End Try
  End Sub

  Private Sub Sp_DataReceived(sender As Object, e As SerialDataReceivedEventArgs)
    Try
      If sp.BytesToRead > 0 Then
        Dim incoming As String = sp.ReadExisting()
        serialBuffer &= incoming

        ' Check if we have a complete line (ends with newline)
        If serialBuffer.Contains(vbLf) Then
          Dim lines = serialBuffer.Split(New String() {vbLf}, StringSplitOptions.RemoveEmptyEntries)

          ' Process all complete lines
          For i = 0 To lines.Length - 1
            Dim line = lines(i).Trim()
            If Not String.IsNullOrEmpty(line) Then
              ' Set the response and signal the event
              SyncLock responseLock
                lastResponse = line
              End SyncLock
              responseEvent.Set()

              '' Detect going offline message and handle it
              'If String.Equals(line, ">Offline", StringComparison.Ordinal) Then
              '  LogFromThread("Device requested going offline. Disconnecting and restarting auto-connect.")
              '  ' Ensure disconnect and auto-reconnect happen on UI thread
              '  Me.BeginInvoke(New Action(AddressOf HandleDeviceGoingOffline))
              '  ' continue processing other lines (if any) but do not re-add handler here since HandleDeviceGoingOffline will close the port
              'End If

            End If
          Next

          ' Keep the last incomplete line in the buffer
          serialBuffer = If(lines.Length > 0 AndAlso serialBuffer.EndsWith(vbLf), "", lines(lines.Length - 1))
        End If
      End If
    Catch ex As Exception
      LogFromThread("DataReceived ERROR: " & ex.Message)
    End Try
  End Sub

  Private Sub HandleDeviceGoingOffline()
    Try
      ' If auto-connect already running, no need to duplicate the effort
      If autoConnectInProgress Then
        Log("Auto-connect already in progress; skipping restart.")
        Thread.Sleep(500)
        Return
      End If

      'statlabel.Text = "Device requested offline. Disconnecting..."
      Log("Device requested offline. Disconnecting...")
      Thread.Sleep(1000)

      Try
        If sp IsNot Nothing Then
          Try
            RemoveHandler sp.DataReceived, AddressOf Sp_DataReceived
          Catch
          End Try
          Try
            If sp.IsOpen Then
              sp.DiscardInBuffer()
              sp.DiscardOutBuffer()
              sp.Close()
            End If
          Catch
          End Try
          Try
            sp.Dispose()
          Catch
          End Try
          sp = Nothing
        End If
      Catch ex As Exception
        Log("Error while disconnecting: " & ex.Message)
        Thread.Sleep(1000)
      End Try

      ' Short pause to allow OS to release the COM port
      Thread.Sleep(300)

      ' Start scanning ports again
      Log("Restarting auto-connect scanning...")
      Thread.Sleep(1000)
      AutoConnectToDataSafe()
    Catch ex As Exception
      Log("HandleDeviceGoingOffline ERROR: " & ex.Message)
      Thread.Sleep(1000)
    End Try
  End Sub

  Private Sub LogFromThread(text As String)
    If Me.InvokeRequired Then
      Me.Invoke(New LogInvoker(AddressOf Log), text)
    Else
      Log(text)
    End If
  End Sub

  Private Sub Log(text As String)
    statlabel.Text = $"[{DateTime.Now:HH:mm:ss}] {text}"
  End Sub

  Private Sub ApplyWindowsTheme()
    Dim isLight = IsWindowsInLightMode()
    If isLight Then
      ApplyTheme(isLight)
    Else
      ApplyTheme(isLight)
    End If
  End Sub

  Private Function IsWindowsInLightMode() As Boolean
    Try
      Using key = Registry.CurrentUser.OpenSubKey("Software\Microsoft\Windows\CurrentVersion\Themes\Personalize", False)
        If key Is Nothing Then
          Return True
        End If
        Dim val = key.GetValue("AppsUseLightTheme")
        If val Is Nothing Then
          Return True
        End If
        Return Convert.ToInt32(val) <> 0
      End Using
    Catch
      ' Do nothing
      Return True
    End Try
  End Function

  Private Sub ApplyTheme(isLight As Boolean)
    If isLight Then
      Me.BackColor = SystemColors.Control
      Me.ForeColor = SystemColors.ControlText
    Else
      Me.BackColor = Color.FromArgb(30, 30, 30)
      Me.ForeColor = Color.White
    End If

    For Each c As Control In Me.Controls
      ApplyThemeToControl(c, isLight)
    Next
  End Sub

  Private Sub ApplyThemeToControl(ctrl As Control, isLight As Boolean)
    If isLight Then
      ctrl.BackColor = SystemColors.Control
      ctrl.ForeColor = SystemColors.ControlText
    Else
      ctrl.BackColor = Color.FromArgb(30, 30, 30)
      ctrl.ForeColor = Color.White
    End If

    ' Special cases
    If TypeOf ctrl Is TextBoxBase Then
      Dim tb = DirectCast(ctrl, TextBoxBase)
      If isLight Then
        tb.BackColor = Color.White
        tb.ForeColor = SystemColors.ControlText
      Else
        tb.BackColor = Color.FromArgb(45, 45, 48)
        tb.ForeColor = Color.White
      End If
    ElseIf TypeOf ctrl Is DataGridView Then
      Dim dgv = DirectCast(ctrl, DataGridView)
      If isLight Then
        dgv.BackgroundColor = SystemColors.Window
        dgv.DefaultCellStyle.BackColor = SystemColors.Window
        dgv.DefaultCellStyle.ForeColor = SystemColors.ControlText
        dgv.ColumnHeadersDefaultCellStyle.BackColor = SystemColors.Control
        dgv.ColumnHeadersDefaultCellStyle.ForeColor = SystemColors.ControlText
        dgv.EnableHeadersVisualStyles = True
      Else
        dgv.BackgroundColor = Color.FromArgb(30, 30, 30)
        dgv.DefaultCellStyle.BackColor = Color.FromArgb(45, 45, 48)
        dgv.DefaultCellStyle.ForeColor = Color.White
        dgv.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(45, 45, 48)
        dgv.ColumnHeadersDefaultCellStyle.ForeColor = Color.White
        dgv.EnableHeadersVisualStyles = False
      End If
    ElseIf TypeOf ctrl Is MenuStrip Then
      Dim ms = DirectCast(ctrl, MenuStrip)
      If isLight Then
        ms.BackColor = SystemColors.Control
        ms.ForeColor = SystemColors.ControlText
      Else
        ms.BackColor = Color.FromArgb(30, 30, 30)
        ms.ForeColor = Color.White
      End If
    ElseIf TypeOf ctrl Is ToolStrip Then
      Dim ts = DirectCast(ctrl, ToolStrip)
      If isLight Then
        ts.BackColor = SystemColors.Control
        ts.ForeColor = SystemColors.ControlText
      Else
        ts.BackColor = Color.FromArgb(30, 30, 30)
        ts.ForeColor = Color.White
      End If
    End If

    For Each child As Control In ctrl.Controls
      ApplyThemeToControl(child, isLight)
    Next
  End Sub

  Protected Overrides Sub WndProc(ByRef m As Message)
    MyBase.WndProc(m)

    Const WM_SETTINGCHANGE As Integer = &H1A
    Const WM_THEMECHANGED As Integer = &H31A

    If m.Msg = WM_SETTINGCHANGE OrElse m.Msg = WM_THEMECHANGED Then
      ApplyWindowsTheme()
    End If
  End Sub

  Private Sub frmMain_Load(sender As Object, e As EventArgs) Handles Me.Load
    AutoConnectToDataSafe()

  End Sub

  ' Handle Action1 clicks: fill username/password and attempt login in active browser tab
  Private Sub dgvDataSafeDataGrid_CellMouseClick(sender As Object, e As DataGridViewCellMouseEventArgs) Handles dgvDataSafeDataGrid.CellMouseClick
    If e.RowIndex < 0 OrElse e.ColumnIndex < 0 Then Return
    Try
      Dim col = dgvDataSafeDataGrid.Columns(e.ColumnIndex)
      If col Is Nothing Then Return
      If col.Name = "Action1" Then
        ' Get username and password from this row (column names expected)
        Dim row = dgvDataSafeDataGrid.Rows(e.RowIndex)
        If row.IsNewRow Then
          Log("Row is new/empty; nothing to send.")
          Return
        End If

        Dim userVal As String = String.Empty
        Dim passVal As String = String.Empty
        Try
          If dgvDataSafeDataGrid.Columns.Contains("UserName") Then
            Dim c = row.Cells("UserName")
            If c IsNot Nothing AndAlso c.Value IsNot Nothing Then userVal = c.Value.ToString()
          End If
        Catch
        End Try
        Try
          If dgvDataSafeDataGrid.Columns.Contains("Password") Then
            Dim c = row.Cells("Password")
            If c IsNot Nothing AndAlso c.Value IsNot Nothing Then passVal = c.Value.ToString()
          End If
        Catch
        End Try

      ElseIf col.Name = "Website" Then
        ' Ensure the clicked cell becomes current, then open the website
        Try
          dgvDataSafeDataGrid.CurrentCell = dgvDataSafeDataGrid.Rows(e.RowIndex).Cells(e.ColumnIndex)
        Catch
          ' ignore if cannot set
        End Try
        OpenWebPage()
      End If
    Catch ex As Exception
      Log("CellMouseClick error: " & ex.Message)
    End Try
  End Sub

  Private Sub btnSend_Click(sender As Object, e As EventArgs) Handles btnSend.Click
    pushItems()
  End Sub

  Private Sub btnLoad_Click(sender As Object, e As EventArgs) Handles btnLoad.Click
    PullItems()
  End Sub

  Private Sub btnRebootPico_Click(sender As Object, e As EventArgs) Handles btnRebootPico.Click
    RebootPico()
    AutoConnectToDataSafe()
  End Sub

  Private Sub btnSaveCSV_Click(sender As Object, e As EventArgs) Handles btnSaveCSV.Click
    ' Ask user for destination and save current DataGridView contents to CSV.
    Using sfd As New SaveFileDialog() With {
      .Title = "Save CSV file",
      .Filter = "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
      .OverwritePrompt = True,
      .AddExtension = True,
      .DefaultExt = "csv"
    }
      If sfd.ShowDialog() <> DialogResult.OK Then
        Return
      End If

      Try
        ' Helper to quote CSV fields (escape quotes by doubling)
        Dim QuoteCsv As Func(Of String, String) = Function(value As String)
                                                    If value Is Nothing Then value = ""
                                                    value = value.Replace("""", """""")
                                                    If value.Contains(","c) OrElse value.Contains(""""c) OrElse value.Contains(vbCr) OrElse value.Contains(vbLf) Then
                                                      Return $"""{value}"""
                                                    End If
                                                    Return value
                                                  End Function

        ' Columns to export in the same order used when sending/loading
        Dim colNames = New String() {"_Name", "Website", "Action1", "UserName", "Password", "Action2", "Ranking"}

        Dim outputLines As New List(Of String)()

        ' Add header
        Dim headerFields As New List(Of String)
        For Each h In colNames
          headerFields.Add(QuoteCsv(h))
        Next
        outputLines.Add(String.Join(",", headerFields.ToArray()))

        ' Iterate DataGridView rows so we can skip the new row
        Dim savedCount As Integer = 0
        For Each dgvr As DataGridViewRow In dgvDataSafeDataGrid.Rows
          If dgvr.IsNewRow Then Continue For

          Dim fields As New List(Of String)
          For Each colName In colNames
            Try
              If dgvDataSafeDataGrid.Columns.Contains(colName) Then
                Dim cell = dgvr.Cells(colName)
                Dim rawVal As String = If(cell IsNot Nothing AndAlso cell.Value IsNot Nothing, cell.Value.ToString(), "")
                rawVal = rawVal.Replace(vbCr, " ").Replace(vbLf, " ")
                fields.Add(QuoteCsv(rawVal))
              Else
                fields.Add(QuoteCsv(String.Empty))
              End If
            Catch
              fields.Add(QuoteCsv(String.Empty))
            End Try
          Next

          outputLines.Add(String.Join(",", fields.ToArray()))
          savedCount += 1
        Next

        File.WriteAllLines(sfd.FileName, outputLines.ToArray(), System.Text.Encoding.UTF8)
        Log($"Saved {savedCount} row(s) to CSV: {Path.GetFileName(sfd.FileName)}")
      Catch ex As Exception
        LogFromThread("Save CSV error: " & ex.Message)
      End Try
    End Using
  End Sub

  Private Sub btnLoadCSV_Click(sender As Object, e As EventArgs) Handles btnLoadCSV.Click
    ' Open file dialog to pick a CSV file (folder browser cannot pick a single file reliably)
    Using ofd As New OpenFileDialog() With {
      .Title = "Select CSV file",
      .Filter = "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
      .CheckFileExists = True,
      .Multiselect = False
    }
      If ofd.ShowDialog() <> DialogResult.OK Then
        Return
      End If

      Try
        Dim lines = File.ReadAllLines(ofd.FileName)
        ' Clear existing rows
        ds.dtAllitems.Clear()

        For Each rawLine In lines
          Dim line = rawLine.Trim()
          If String.IsNullOrEmpty(line) Then Continue For

          Dim row = ds.dtAllitems.NewRow()
          row("Raw") = line

          Dim parts = line.Split(","c)
          For i As Integer = 0 To parts.Length - 1
            ' Ensure we don't index past available columns
            If ds.dtAllitems.Columns.Count > i + 1 Then
              row(i + 1) = parts(i).Trim()
            End If
          Next

          ds.dtAllitems.Rows.Add(row)
        Next
        If lines.Length > 0 Then
          btnSaveCSV.Enabled = True
        Else
          btnSaveCSV.Enabled = False
        End If

        ' Pre-populate the insert/new row Action1/Action2 cells if present
        Dim newIdx As Integer = dgvDataSafeDataGrid.NewRowIndex
        If newIdx >= 0 AndAlso newIdx < dgvDataSafeDataGrid.Rows.Count Then
          Try
            dgvDataSafeDataGrid.Rows(newIdx).Cells.Item("Action1").Value = "Login..."
            dgvDataSafeDataGrid.Rows(newIdx).Cells.Item("Action2").Value = "Return..."
          Catch
            ' ignore if columns/cells not available
          End Try
        End If

        Log($"Loaded {ds.dtAllitems.Rows.Count} row(s) from CSV: {Path.GetFileName(ofd.FileName)}")
      Catch ex As Exception
        LogFromThread("Load CSV error: " & ex.Message)
      End Try
    End Using
  End Sub

  Private Sub btnResync_Click(sender As Object, e As EventArgs) Handles btnResync.Click
    resyncPico()
  End Sub

  Private Sub tmrCheckComPort_Tick(sender As Object, e As EventArgs) Handles tmrCheckComPort.Tick

    tmrCheckComPort.Stop()

    If sp Is Nothing OrElse Not sp.IsOpen Then
      If sp IsNot Nothing Then
        Try
          sp.Close()
        Catch ex As Exception
          ' do nothing
        End Try
        sp.Dispose()
        sp = Nothing
        Thread.Sleep(500) ' Allow time for cleanup
      End If
      ' Create new connection
      ConnectToPort(portUsed)
    End If

    tmrCheckComPort.Start()

  End Sub

  Private Sub dgvDataSafeDataGrid_CellBeginEdit(sender As Object, e As DataGridViewCellCancelEventArgs) Handles dgvDataSafeDataGrid.CellBeginEdit

    Dim dgv = DirectCast(sender, DataGridView)

    ' Pre-populate the insert/new row Action1/Action2 cells if present
    If dgv.Rows(e.RowIndex).IsNewRow Then
      Try
        dgvDataSafeDataGrid.Rows(e.RowIndex).Cells.Item("Action1").Value = "Login..."
        dgvDataSafeDataGrid.Rows(e.RowIndex).Cells.Item("Action2").Value = "Return..."
      Catch
        ' ignore if columns/cells not available
      End Try

    End If
  End Sub

  Private Sub dgvDataSafeDataGrid_Sorted(sender As Object, e As EventArgs) Handles dgvDataSafeDataGrid.Sorted
    'Set column Ranking values to 0 after sorting
    For Each row As DataGridViewRow In dgvDataSafeDataGrid.Rows
      If Not row.IsNewRow Then
        Try
          row.Cells("Ranking").Value = "0"
        Catch
          ' ignore
        End Try
      End If
    Next
  End Sub

  Private Sub btnLoad_MouseHover(sender As Object, e As EventArgs) Handles btnLoad.MouseHover
    With ToolTip1
      .SetToolTip(btnLoad, "Fetch all items from DataSafe device.")
    End With
  End Sub

  Private Sub btnSend_MouseHover(sender As Object, e As EventArgs) Handles btnSend.MouseHover
    With ToolTip1
      .SetToolTip(btnSend, "Send all items in the grid to DataSafe device.")
    End With
  End Sub

  Private Sub btnRebootPico_MouseHover(sender As Object, e As EventArgs) Handles btnRebootPico.MouseHover
    With ToolTip1
      .SetToolTip(btnRebootPico, "Reboot the DataSafe device.")
    End With
  End Sub

  Private Sub btnResync_MouseHover(sender As Object, e As EventArgs) Handles btnResync.MouseHover
    With ToolTip1
      .SetToolTip(btnResync, "Resynchronize the Time on the DataSafe device.")
    End With
  End Sub

  Private Sub btnLoadCSV_MouseHover(sender As Object, e As EventArgs) Handles btnLoadCSV.MouseHover
    With ToolTip1
      .SetToolTip(btnLoadCSV, "Load items from a CSV file into the grid.")
    End With
  End Sub

  Private Sub btnSaveCSV_MouseHover(sender As Object, e As EventArgs) Handles btnSaveCSV.MouseHover
    With ToolTip1
      .SetToolTip(btnSaveCSV, "Save items from the grid into a CSV file.")
    End With
  End Sub

  Private Sub mtbPin_MouseHover(sender As Object, e As EventArgs) Handles mtbPin.MouseHover
    With ToolTip1
      .SetToolTip(mtbPin, "Enter the PIN code used by the DataSafe device.")
    End With
  End Sub

  Private Sub pbSearch_MouseClick(sender As Object, e As MouseEventArgs) Handles pbSearch.MouseClick
    'Search the txtSearch.text item in the datagrid control _Name collumn and when found select the row and scroll to it

    Dim searchText As String = txtSearch.Text.Trim().ToLower()
    If String.IsNullOrEmpty(searchText) Then
      Return
    End If

    For Each row As DataGridViewRow In dgvDataSafeDataGrid.Rows
      If Not row.IsNewRow Then
        Try
          Dim cellValue As String = If(row.Cells("_Name").Value IsNot Nothing, row.Cells("_Name").Value.ToString().ToLower(), "")
          If cellValue.Contains(searchText) Then
            row.Selected = True
            dgvDataSafeDataGrid.FirstDisplayedScrollingRowIndex = row.Index
            Return
          End If
        Catch
          ' ignore
        End Try
      End If
    Next



  End Sub

  Private Sub dgvDataSafeDataGrid_CellFormatting(sender As Object, e As DataGridViewCellFormattingEventArgs) Handles dgvDataSafeDataGrid.CellFormatting
    ' Apply password masking for Password column
    If dgvDataSafeDataGrid.Columns(e.ColumnIndex).Name = "Password" Then
      e.FormattingApplied = True

      Dim actual As String = ""
      Try
        If e.Value IsNot Nothing AndAlso Not Convert.IsDBNull(e.Value) Then
          actual = e.Value.ToString()
        End If
      Catch
        actual = ""
      End Try

      If passwordRevealActive AndAlso e.RowIndex = passwordRevealRow AndAlso e.ColumnIndex = passwordRevealCol Then
        ' During reveal, show the actual password
        e.Value = actual
      Else
        ' Normal masked display
        If actual.Length > 0 Then
          e.Value = New String("●"c, actual.Length)
        Else
          e.Value = String.Empty
        End If
      End If
    End If
  End Sub

  Private Sub dgvDataSafeDataGrid_MouseDown(sender As Object, e As MouseEventArgs) Handles dgvDataSafeDataGrid.MouseDown
    ' Middle click behavior: type/paste current cell contents into the active tab of the user's current browser
    If e.Button = MouseButtons.Middle Then
      Dim hit = dgvDataSafeDataGrid.HitTest(e.X, e.Y)
      If hit.Type = DataGridViewHitTestType.Cell AndAlso hit.RowIndex >= 0 AndAlso (hit.ColumnIndex = 2 Or hit.ColumnIndex = 4 Or hit.ColumnIndex = 5) Then
        Try
          Dim cell = dgvDataSafeDataGrid.Rows(hit.RowIndex).Cells(hit.ColumnIndex)
          Dim txt As String = If(cell IsNot Nothing AndAlso cell.Value IsNot Nothing, cell.Value.ToString(), String.Empty)
          If String.IsNullOrEmpty(txt) Then
            statlabel.Text = "Cell empty — nothing to send."
            Return
          End If
          statlabel.Text = "Sending cell contents to browser..."
          SendTextToBrowserClipboardPaste(txt)
        Catch ex As Exception
          Log("Error sending cell to browser: " & ex.Message)
        End Try
      End If
    End If
  End Sub

  Private Sub dgvDataSafeDataGrid_CellMouseEnter(sender As Object, e As DataGridViewCellEventArgs) Handles dgvDataSafeDataGrid.CellMouseEnter
    If e.RowIndex >= 0 AndAlso e.ColumnIndex >= 0 Then
      Dim col = dgvDataSafeDataGrid.Columns(e.ColumnIndex)
      If col IsNot Nothing AndAlso col.Name = "Password" Then
        passwordRevealActive = True
        passwordRevealRow = e.RowIndex
        passwordRevealCol = e.ColumnIndex
        dgvDataSafeDataGrid.InvalidateCell(dgvDataSafeDataGrid.Rows(e.RowIndex).Cells(e.ColumnIndex))
      End If
    End If
  End Sub

  Private Sub dgvDataSafeDataGrid_CellMouseLeave(sender As Object, e As DataGridViewCellEventArgs) Handles dgvDataSafeDataGrid.CellMouseLeave
    If e.RowIndex >= 0 AndAlso e.ColumnIndex >= 0 Then
      Dim col = dgvDataSafeDataGrid.Columns(e.ColumnIndex)
      If col IsNot Nothing AndAlso col.Name = "Password" Then
        passwordRevealActive = False
        passwordRevealRow = e.RowIndex
        passwordRevealCol = e.ColumnIndex
        dgvDataSafeDataGrid.InvalidateCell(dgvDataSafeDataGrid.Rows(e.RowIndex).Cells(e.ColumnIndex))
      End If
    End If
  End Sub

  Private Sub mtbPin_TextChanged(sender As Object, e As EventArgs) Handles mtbPin.TextChanged
    If mtbPin.Text.Length = 4 AndAlso deviceConnected Then
      btnLoad.Enabled = True
      btnLoadCSV.Enabled = True
      btnRebootPico.Enabled = True
      btnResync.Enabled = True
    Else
      btnLoad.Enabled = False
      btnLoadCSV.Enabled = False
      btnRebootPico.Enabled = False
      btnResync.Enabled = False
    End If
  End Sub

  Public Sub PullItems()
    If sp Is Nothing OrElse Not sp.IsOpen Then
      'MessageBox.Show("")
      statlabel.Text = "Not connected to DataSafe. Restart DataSafe device into Utility mode!"
      Return
    End If

    With tmrCheckComPort
      .Stop()
      .Enabled = False
    End With

    ' Capture pin on UI thread to avoid cross-thread access
    Dim pin As String = mtbPin.Text.Trim()

    ' Start background worker so UI stays responsive
    Dim t As New Thread(Sub()
                          Dim originalHandler As SerialDataReceivedEventHandler = AddressOf Sp_DataReceived
                          Dim localHandler As SerialDataReceivedEventHandler = Nothing
                          Dim lines As New List(Of String)()
                          Dim localBuffer As String = ""
                          Dim lastReceived As DateTime = DateTime.UtcNow
                          Dim lockObj As New Object()
                          Dim inactivityMs As Integer = 800
                          Dim totalTimeoutMs As Integer = 15000

                          Try
                            ' Temporarily replace the main handler to avoid two readers fighting for the port buffer.
                            Try
                              RemoveHandler sp.DataReceived, originalHandler
                            Catch
                              'Do nothing
                            End Try

                            localHandler = Sub(sender As Object, e As SerialDataReceivedEventArgs)
                                             Try
                                               If sp.BytesToRead > 0 Then
                                                 Dim inc As String = sp.ReadExisting()
                                                 SyncLock lockObj
                                                   localBuffer &= inc
                                                   ' extract complete lines ending with vbLf
                                                   While localBuffer.Contains(vbLf)
                                                     Dim idx = localBuffer.IndexOf(vbLf)
                                                     Dim line = localBuffer.Substring(0, idx).Trim()
                                                     localBuffer = If(idx + 1 < localBuffer.Length, localBuffer.Substring(idx + 1), "")
                                                     If Not String.IsNullOrEmpty(line) Then
                                                       lines.Add(line)
                                                     End If
                                                     lastReceived = DateTime.UtcNow
                                                   End While
                                                 End SyncLock
                                               End If
                                             Catch ex As Exception
                                               'Do nothing
                                             End Try
                                           End Sub

                            AddHandler sp.DataReceived, localHandler

                            ' Clear any stale input and ask device to type out (include PIN)
                            Try
                              sp.DiscardInBuffer()
                            Catch
                            End Try

                            Try
                              sp.WriteLine($"STREAMOUT,{pin}")
                              LogFromThread($"> STREAMOUT,{pin}")
                            Catch ex As Exception
                              LogFromThread("STREAMOUT send error: " & ex.Message)
                              Return
                            End Try

                            ' Wait until either inactivity window elapses or total timeout reached
                            Dim sw As Stopwatch = Stopwatch.StartNew()
                            While sw.ElapsedMilliseconds < totalTimeoutMs
                              Thread.Sleep(150)
                              Dim sinceLast As TimeSpan
                              SyncLock lockObj
                                sinceLast = DateTime.UtcNow - lastReceived
                              End SyncLock
                              If lines.Count > 0 AndAlso sinceLast.TotalMilliseconds > inactivityMs Then
                                Exit While
                              End If
                            End While

                          Finally
                            ' Restore original handler
                            Try
                              If localHandler IsNot Nothing Then
                                RemoveHandler sp.DataReceived, localHandler
                              End If
                            Catch
                            End Try
                            Try
                              AddHandler sp.DataReceived, originalHandler
                            Catch
                            End Try
                          End Try

                          ' Populate dataset on UI thread
                          Me.Invoke(Sub()
                                      Try
                                        ' Clear existing rows
                                        ds.dtAllitems.Clear()

                                        ' Determine max fields across received lines
                                        Dim maxFields As Integer = 0
                                        For Each l In lines
                                          Dim cnt = l.Split(","c).Length
                                          If cnt > maxFields Then maxFields = cnt
                                        Next

                                        ' Add rows
                                        For Each l In lines
                                          Dim row = ds.dtAllitems.NewRow()
                                          row("Raw") = l
                                          Dim parts = l.Split(","c)
                                          For i As Integer = 0 To parts.Length - 1
                                            row(i + 1) = parts(i).Trim()
                                          Next
                                          ds.dtAllitems.Rows.Add(row)
                                        Next
                                        'Pre-populate new row Action1/2 collumns with predefined content
                                        dgvDataSafeDataGrid.Rows(dgvDataSafeDataGrid.NewRowIndex).Cells.Item("Action1").Value = "Login..."
                                        dgvDataSafeDataGrid.Rows(dgvDataSafeDataGrid.NewRowIndex).Cells.Item("Action2").Value = "Return..."
                                        Log($"Pulled in {lines.Count} item(s).")
                                        If mtbPin.Text.Length = 4 AndAlso deviceConnected Then
                                          btnSend.Enabled = True
                                          btnSaveCSV.Enabled = True
                                        Else
                                          btnSend.Enabled = False
                                        End If
                                      Catch ex As Exception
                                        Log("Error populating dsAllitems: " & ex.Message)
                                      End Try

                                      With tmrCheckComPort
                                        .Enabled = True
                                        .Start()
                                      End With

                                    End Sub)
                        End Sub) With {.IsBackground = True}
    t.Start()

  End Sub

  Public Sub pushItems()
    If sp Is Nothing OrElse Not sp.IsOpen Then
      statlabel.Text = "Not connected to DataSafe. Cannot send."
      Return
    End If

    ' Capture pin on UI thread to avoid cross-thread access
    Dim pin As String = mtbPin.Text.Trim()

    ' Make a snapshot of rows to send to avoid cross-thread table changes while sending.
    Dim rowsToSend = ds.dtAllitems.Rows.Cast(Of DataRow)().Where(Function(r) r.RowState <> DataRowState.Deleted).ToList()

    If rowsToSend.Count = 0 Then
      statlabel.Text = "Nothing to send."
      Return
    End If

    ' Disable UI while sending
    btnSend.Enabled = False
    btnLoad.Enabled = False
    dgvDataSafeDataGrid.Enabled = False



    Dim sendThread As New Thread(Sub()
                                   Dim originalHandler As SerialDataReceivedEventHandler = AddressOf Sp_DataReceived
                                   Dim localHandler As SerialDataReceivedEventHandler = Nothing
                                   Dim localBuffer As String = ""
                                   Dim lockObj As New Object()
                                   Dim ackTimeoutMs As Integer = 5000

                                   Try
                                     ' Temporarily replace the main handler to avoid handler conflicts
                                     Try
                                       RemoveHandler sp.DataReceived, originalHandler
                                     Catch
                                     End Try

                                     ' Local handler collects complete lines and signals responseEvent
                                     localHandler = Sub(sender As Object, e As SerialDataReceivedEventArgs)
                                                      Try
                                                        If sp.BytesToRead > 0 Then
                                                          Dim inc As String = sp.ReadExisting()
                                                          SyncLock lockObj
                                                            localBuffer &= inc
                                                            While localBuffer.Contains(vbLf)
                                                              Dim idx = localBuffer.IndexOf(vbLf)
                                                              Dim line = localBuffer.Substring(0, idx).Trim()
                                                              localBuffer = If(idx + 1 < localBuffer.Length, localBuffer.Substring(idx + 1), "")
                                                              If Not String.IsNullOrEmpty(line) Then
                                                                SyncLock responseLock
                                                                  lastResponse = line
                                                                End SyncLock
                                                                responseEvent.Set()
                                                              End If
                                                            End While
                                                          End SyncLock
                                                        End If
                                                      Catch ex As Exception
                                                        ' ignore
                                                      End Try
                                                    End Sub

                                     AddHandler sp.DataReceived, localHandler

                                     ' Clear any stale input and prepare for ack
                                     Try
                                       sp.DiscardInBuffer()
                                     Catch
                                       'Do nothing
                                     End Try

                                     ' Reset response event and lastResponse before sending STREAMIN
                                     responseEvent.Reset()
                                     SyncLock responseLock
                                       lastResponse = ""
                                     End SyncLock

                                     ' Send STREAMIN command with pin and wait for ack
                                     Try
                                       sp.WriteLine($"STREAMIN,{pin}")
                                       LogFromThread($"> STREAMIN,{pin}")
                                     Catch ex As Exception
                                       LogFromThread("STREAMIN send error: " & ex.Message)
                                       Return
                                     End Try

                                     ' Wait for acknowledgment (any single line response within timeout)
                                     Dim ackReceived As Boolean = False
                                     Dim swAck As Stopwatch = Stopwatch.StartNew()
                                     While swAck.ElapsedMilliseconds < ackTimeoutMs
                                       If responseEvent.WaitOne(200) Then
                                         responseEvent.Reset()
                                         Dim resp As String = ""
                                         SyncLock responseLock
                                           resp = lastResponse
                                         End SyncLock
                                         If Not String.IsNullOrEmpty(resp) Then
                                           ackReceived = True
                                           LogFromThread($"Ack received: {resp}")
                                           Exit While
                                         End If
                                       End If
                                     End While

                                     If Not ackReceived Then
                                       LogFromThread("No acknowledgment from device for STREAMIN. Aborting send.")
                                       Me.Invoke(Sub() statlabel.Text = "No STREAMIN ack received. Send aborted.")
                                       Return
                                     End If

                                     ' Proceed to send each CSV row (device is expected to accept lines terminated by vbLf)
                                     LogFromThread($"Sending {rowsToSend.Count} row(s) to device...")
                                     Dim colNames = New String() {"_Name", "Website", "Action1", "UserName", "Password", "Action2", "Ranking"}

                                     Dim index As Integer = 0
                                     For Each dr In rowsToSend
                                       index += 1
                                       Dim fields As New List(Of String)
                                       For Each col In colNames
                                         Dim val As String = ""
                                         Try
                                           If ds.dtAllitems.Columns.Contains(col) Then
                                             Dim obj = dr(col)
                                             If obj IsNot Nothing AndAlso obj IsNot DBNull.Value Then
                                               val = obj.ToString()
                                             End If
                                           End If
                                         Catch
                                           val = ""
                                         End Try
                                         val = val.Replace(vbCr, " ").Replace(vbLf, " ")
                                         fields.Add(val)
                                       Next

                                       Dim csvLine = String.Join(",", fields)
                                       Try
                                         sp.WriteLine(csvLine)
                                         LogFromThread($"> {csvLine}")
                                       Catch ex As Exception
                                         LogFromThread("Send error: " & ex.Message)
                                         Exit For
                                       End Try

                                       Thread.Sleep(30)
                                       Me.Invoke(Sub() statlabel.Text = $"Sending {index}/{rowsToSend.Count}...")
                                     Next

                                     LogFromThread("Send complete.")
                                     Me.Invoke(Sub() statlabel.Text = $"Send complete. Sent {rowsToSend.Count} row(s).")

                                   Finally
                                     ' Restore original handler
                                     Try
                                       If localHandler IsNot Nothing Then
                                         RemoveHandler sp.DataReceived, localHandler
                                       End If
                                     Catch
                                     End Try
                                     Try
                                       AddHandler sp.DataReceived, originalHandler
                                     Catch
                                     End Try

                                     ' Re-enable UI
                                     Me.Invoke(Sub()
                                                 btnSend.Enabled = True
                                                 btnLoad.Enabled = True
                                                 dgvDataSafeDataGrid.Enabled = True
                                               End Sub)
                                   End Try
                                 End Sub) With {.IsBackground = True}
    sendThread.Start()

  End Sub

  Public Sub RebootPico()
    If sp Is Nothing OrElse Not sp.IsOpen Then
      statlabel.Text = "Not connected to DataSafe. Cannot reboot."
      Return
    End If

    ' Disable reboot button briefly
    Try
      btnRebootPico.Enabled = False
    Catch
    End Try

    ' Capture pin on UI thread to avoid cross-thread access
    Dim pin As String = mtbPin.Text.Trim()

    Dim t As New Thread(Sub()
                          Dim originalHandler As SerialDataReceivedEventHandler = AddressOf Sp_DataReceived
                          Dim localHandler As SerialDataReceivedEventHandler = Nothing
                          Dim localBuffer As String = ""
                          Dim lockObj As New Object()
                          Dim ackTimeoutMs As Integer = 5000

                          Try
                            ' Temporarily replace main handler
                            Try
                              RemoveHandler sp.DataReceived, originalHandler
                            Catch
                            End Try

                            localHandler = Sub(sender As Object, e As SerialDataReceivedEventArgs)
                                             Try
                                               If sp.BytesToRead > 0 Then
                                                 Dim inc As String = sp.ReadExisting()
                                                 SyncLock lockObj
                                                   localBuffer &= inc
                                                   While localBuffer.Contains(vbLf)
                                                     Dim idx = localBuffer.IndexOf(vbLf)
                                                     Dim line = localBuffer.Substring(0, idx).Trim()
                                                     localBuffer = If(idx + 1 < localBuffer.Length, localBuffer.Substring(idx + 1), "")
                                                     If Not String.IsNullOrEmpty(line) Then
                                                       SyncLock responseLock
                                                         lastResponse = line
                                                       End SyncLock
                                                       responseEvent.Set()
                                                       ' Also log incoming lines locally for visibility
                                                       LogFromThread($"< {line}")
                                                     End If
                                                   End While
                                                 End SyncLock
                                               End If
                                             Catch ex As Exception
                                               ' ignore
                                             End Try
                                           End Sub

                            AddHandler sp.DataReceived, localHandler

                            ' Clear any stale input
                            Try
                              sp.DiscardInBuffer()
                            Catch
                            End Try

                            ' Reset response event and lastResponse
                            responseEvent.Reset()
                            SyncLock responseLock
                              lastResponse = ""
                            End SyncLock

                            ' Send REBOOT command
                            Try
                              sp.WriteLine($"REBOOT,{pin}")
                              LogFromThread("> REBOOT")
                              Me.Invoke(Sub() statlabel.Text = "Sent REBOOT, waiting for ack...")
                            Catch ex As Exception
                              LogFromThread("REBOOT send error: " & ex.Message)
                              Return
                            End Try

                            ' Wait for single-line ack (within timeout)
                            Dim ackReceived As Boolean = False
                            Dim respText As String = ""
                            Dim sw As Stopwatch = Stopwatch.StartNew()
                            While sw.ElapsedMilliseconds < ackTimeoutMs
                              If responseEvent.WaitOne(200) Then
                                responseEvent.Reset()
                                Dim resp As String = ""
                                SyncLock responseLock
                                  resp = lastResponse
                                End SyncLock
                                If Not String.IsNullOrEmpty(resp) Then
                                  ackReceived = True
                                  respText = resp
                                  Exit While
                                End If
                              End If
                            End While

                            If ackReceived Then
                              LogFromThread($"REBOOT ack received: {respText}")
                              Me.Invoke(Sub() statlabel.Text = $"REBOOT ack: {respText}")
                            Else
                              LogFromThread("No ack received for REBOOT (timeout).")
                              Me.Invoke(Sub() statlabel.Text = "No REBOOT ack received (timeout).")
                            End If

                          Finally
                            ' Restore original handler
                            Try
                              If localHandler IsNot Nothing Then
                                RemoveHandler sp.DataReceived, localHandler
                              End If
                            Catch
                            End Try
                            Try
                              AddHandler sp.DataReceived, originalHandler
                            Catch
                            End Try

                            ' Re-enable reboot button
                            Me.Invoke(Sub()
                                        Try
                                          btnRebootPico.Enabled = True
                                        Catch
                                        End Try
                                      End Sub)
                          End Try
                        End Sub) With {.IsBackground = True}
    t.Start()
  End Sub

  ' open website in existing browser (focus address bar, paste & enter) or fallback to default launch
  Private Sub OpenWebPage()
    Dim url As String = String.Empty
    Try
      ' Prefer current cell if it is Website; otherwise look up Website column on the current row
      Dim curCell As DataGridViewCell = dgvDataSafeDataGrid.CurrentCell
      If curCell Is Nothing OrElse curCell.RowIndex < 0 Then
        Me.BeginInvoke(Sub() Log("No selected row to open website from."))
        Return
      End If

      Dim websiteCell As DataGridViewCell = Nothing
      If dgvDataSafeDataGrid.Columns(curCell.ColumnIndex).Name = "Website" Then
        websiteCell = curCell
      Else
        If dgvDataSafeDataGrid.Columns.Contains("Website") Then
          websiteCell = dgvDataSafeDataGrid.Rows(curCell.RowIndex).Cells("Website")
        End If
      End If

      If websiteCell Is Nothing Then
        Me.BeginInvoke(Sub() Log("Website column not available."))
        Return
      End If

      If websiteCell.Value Is Nothing Then
        Me.BeginInvoke(Sub() Log("Website cell is empty."))
        Return
      End If

      url = websiteCell.Value.ToString().Trim()
      If String.IsNullOrEmpty(url) Then
        Me.BeginInvoke(Sub() Log("Website URL is empty."))
        Return
      End If

      ' Add scheme if missing (prefer https)
      If Not url.StartsWith("http://", StringComparison.OrdinalIgnoreCase) AndAlso Not url.StartsWith("https://", StringComparison.OrdinalIgnoreCase) Then
        url = "https://" & url
      End If

    Catch ex As Exception
      Me.BeginInvoke(Sub() Log("OpenWebPage error reading cell: " & ex.Message))
      Return
    End Try

    ' Try to find a running browser window to reuse
    Dim targetHwnd As IntPtr = ForegroundWindowIsBrowser()
    If targetHwnd = IntPtr.Zero Then
      targetHwnd = FindAnyBrowserWindowHandle()
    End If

    If targetHwnd = IntPtr.Zero Then
      ' No browser found — fall back to normal Process.Start which opens default browser/new window/tab
      Try
        Process.Start(New ProcessStartInfo(url) With {.UseShellExecute = True})
        Me.BeginInvoke(Sub() statlabel.Text = "Opened URL in default browser.")
      Catch ex As Exception
        Me.BeginInvoke(Sub() Log("Failed to open URL: " & ex.Message))
      End Try
      Return
    End If

    ' Use STA thread for clipboard + SendKeys interaction
    Dim t As New Thread(Sub()
                          Try
                            Dim origData As IDataObject = Nothing
                            Try
                              origData = Clipboard.GetDataObject()
                            Catch
                              origData = Nothing
                            End Try

                            ' Place URL on clipboard
                            Try
                              Clipboard.Clear()
                              Clipboard.SetText(url)
                            Catch ex As Exception
                              Me.BeginInvoke(Sub() Log("Clipboard set failed: " & ex.Message))
                              Return
                            End Try

                            ' Bring browser to foreground (if not already) so address bar focus works
                            Dim fg = GetForegroundWindow()
                            If fg <> targetHwnd Then
                              ForceSetForegroundWindow(targetHwnd)
                              Thread.Sleep(150)
                            Else
                              Thread.Sleep(80)
                            End If

                            ' Focus address bar and paste URL then Enter
                            Try
                              ' Ctrl+L to focus address bar in major browsers
                              SendKeys.SendWait("^{l}")
                              Thread.Sleep(80)
                              SendKeys.SendWait("^{v}")
                              Thread.Sleep(60)
                              SendKeys.SendWait("{ENTER}")
                            Catch ex As Exception
                              Me.BeginInvoke(Sub() Log("SendKeys failed: " & ex.Message))
                            End Try

                            Thread.Sleep(120)

                            ' Restore original clipboard if present
                            If origData IsNot Nothing Then
                              Try
                                Clipboard.SetDataObject(origData, True)
                              Catch
                                ' ignore
                              End Try
                            End If

                            Me.BeginInvoke(Sub() statlabel.Text = "Opened website: " & url)
                          Catch ex As Exception
                            Me.BeginInvoke(Sub() Log("OpenWebPage error: " & ex.Message))
                          End Try
                        End Sub)
    t.SetApartmentState(ApartmentState.STA)
    t.IsBackground = True
    t.Start()
  End Sub
  ' --- end OpenWebPage ---



End Class
