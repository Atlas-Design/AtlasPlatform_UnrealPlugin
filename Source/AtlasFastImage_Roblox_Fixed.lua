-- AtlasFastImage.plugin.lua (Fixed for Roblox)
-- Mirrors your Python workflow: submit async, poll status, get result.
--
-- LIMITATION: Roblox cannot display arbitrary external images in ImageLabel.
-- WORKFLOW: Generate → Copy URL from Output → Paste in browser → Download PNG
--           → Asset Manager → Bulk Import → Use rbxassetid:// in game

local HttpService = game:GetService("HttpService")

-- =========================
-- Config (same as Python)
-- =========================
local API_URL_BASE = "https://api.prod.atlas.design"
local API_VERSION = "0.1"
local API_ID = "1fbd9bd4-d3da-4e95-8fd2-b00c1282d67d"

local DEFAULT_POLL_INTERVAL = 2.0

-- =========================
-- Toolbar + Dock UI
-- =========================
local toolbar = plugin:CreateToolbar("Atlas")
local toggleButton = toolbar:CreateButton(
	"Atlas FastImage",
	"Open Atlas FastImage Tool",
	"rbxassetid://4458901886"
)

local widgetInfo = DockWidgetPluginGuiInfo.new(
	Enum.InitialDockState.Right,
	true,
	false,
	420,
	520,
	320,
	300
)

local widget = plugin:CreateDockWidgetPluginGui("AtlasFastImageWidget", widgetInfo)
widget.Title = "Atlas FastImage"

local root = Instance.new("Frame")
root.Size = UDim2.fromScale(1, 1)
root.BackgroundTransparency = 1
root.Parent = widget

local padding = Instance.new("UIPadding")
padding.PaddingTop = UDim.new(0, 10)
padding.PaddingLeft = UDim.new(0, 10)
padding.PaddingRight = UDim.new(0, 10)
padding.PaddingBottom = UDim.new(0, 10)
padding.Parent = root

local layout = Instance.new("UIListLayout")
layout.FillDirection = Enum.FillDirection.Vertical
layout.SortOrder = Enum.SortOrder.LayoutOrder
layout.Padding = UDim.new(0, 8)
layout.Parent = root

local title = Instance.new("TextLabel")
title.LayoutOrder = 1
title.Size = UDim2.new(1, 0, 0, 18)
title.BackgroundTransparency = 1
title.TextXAlignment = Enum.TextXAlignment.Left
title.Font = Enum.Font.SourceSansBold
title.TextSize = 16
title.Text = "Prompt"
title.TextColor3 = Color3.fromRGB(255, 255, 255)
title.Parent = root

local inputBox = Instance.new("TextBox")
inputBox.LayoutOrder = 2
inputBox.Size = UDim2.new(1, 0, 0, 36)
inputBox.TextXAlignment = Enum.TextXAlignment.Left
inputBox.ClearTextOnFocus = false
inputBox.PlaceholderText = "Describe the image… (empty is allowed)"
inputBox.Text = ""
inputBox.TextColor3 = Color3.fromRGB(255, 255, 255)
inputBox.BackgroundColor3 = Color3.fromRGB(50, 50, 50)
inputBox.Parent = root

local pollLabel = Instance.new("TextLabel")
pollLabel.LayoutOrder = 3
pollLabel.Size = UDim2.new(1, 0, 0, 18)
pollLabel.BackgroundTransparency = 1
pollLabel.TextXAlignment = Enum.TextXAlignment.Left
pollLabel.Font = Enum.Font.SourceSans
pollLabel.TextSize = 14
pollLabel.Text = "Poll interval (seconds)"
pollLabel.TextColor3 = Color3.fromRGB(200, 200, 200)
pollLabel.Parent = root

local pollBox = Instance.new("TextBox")
pollBox.LayoutOrder = 4
pollBox.Size = UDim2.new(1, 0, 0, 28)
pollBox.TextXAlignment = Enum.TextXAlignment.Left
pollBox.ClearTextOnFocus = false
pollBox.Text = tostring(DEFAULT_POLL_INTERVAL)
pollBox.TextColor3 = Color3.fromRGB(255, 255, 255)
pollBox.BackgroundColor3 = Color3.fromRGB(50, 50, 50)
pollBox.Parent = root

local runButton = Instance.new("TextButton")
runButton.LayoutOrder = 5
runButton.Size = UDim2.new(1, 0, 0, 34)
runButton.Text = "Generate"
runButton.TextColor3 = Color3.fromRGB(255, 255, 255)
runButton.BackgroundColor3 = Color3.fromRGB(0, 120, 215)
runButton.Parent = root

local statusLabel = Instance.new("TextLabel")
statusLabel.LayoutOrder = 6
statusLabel.Size = UDim2.new(1, 0, 0, 18)
statusLabel.BackgroundTransparency = 1
statusLabel.TextXAlignment = Enum.TextXAlignment.Left
statusLabel.Font = Enum.Font.SourceSansItalic
statusLabel.TextSize = 14
statusLabel.Text = "Idle."
statusLabel.TextColor3 = Color3.fromRGB(180, 180, 180)
statusLabel.Parent = root

local metaBox = Instance.new("TextBox")
metaBox.LayoutOrder = 7
metaBox.Size = UDim2.new(1, 0, 0, 180)
metaBox.TextXAlignment = Enum.TextXAlignment.Left
metaBox.TextYAlignment = Enum.TextYAlignment.Top
metaBox.ClearTextOnFocus = false
metaBox.MultiLine = true
metaBox.TextWrapped = true
metaBox.TextEditable = false
metaBox.Text = ""
metaBox.TextColor3 = Color3.fromRGB(220, 220, 220)
metaBox.BackgroundColor3 = Color3.fromRGB(40, 40, 40)
metaBox.Parent = root

-- "Print to Output" button
local printButton = Instance.new("TextButton")
printButton.LayoutOrder = 8
printButton.Size = UDim2.new(1, 0, 0, 34)
printButton.Text = "Print URL to Output"
printButton.TextColor3 = Color3.fromRGB(255, 255, 255)
printButton.BackgroundColor3 = Color3.fromRGB(0, 100, 180)
printButton.Visible = false
printButton.Parent = root

-- "Show History" button
local historyButton = Instance.new("TextButton")
historyButton.LayoutOrder = 9
historyButton.Size = UDim2.new(1, 0, 0, 28)
historyButton.Text = "Print History to Output"
historyButton.TextColor3 = Color3.fromRGB(200, 200, 200)
historyButton.BackgroundColor3 = Color3.fromRGB(60, 60, 60)
historyButton.Parent = root

-- Instructions label
local instructionsLabel = Instance.new("TextLabel")
instructionsLabel.LayoutOrder = 10
instructionsLabel.Size = UDim2.new(1, 0, 0, 50)
instructionsLabel.BackgroundTransparency = 1
instructionsLabel.TextXAlignment = Enum.TextXAlignment.Left
instructionsLabel.TextYAlignment = Enum.TextYAlignment.Top
instructionsLabel.Font = Enum.Font.SourceSans
instructionsLabel.TextSize = 12
instructionsLabel.TextWrapped = true
instructionsLabel.TextColor3 = Color3.fromRGB(150, 150, 150)
instructionsLabel.Text = "To import: Copy URL → browser → download PNG\nThen: Asset Manager → Bulk Import → select file"
instructionsLabel.Parent = root

-- Store last result URL and history
local lastDownloadUrl = nil
local generationHistory = {} -- Store recent generations

-- =========================
-- Helpers
-- =========================
local function safeJsonDecode(text)
	local ok, parsed = pcall(function()
		return HttpService:JSONDecode(text)
	end)
	if ok then return parsed end
	return nil
end

local function prettyJson(value)
	local ok, encoded = pcall(function()
		return HttpService:JSONEncode(value)
	end)
	return ok and encoded or tostring(value)
end

local function parsePollInterval()
	local n = tonumber(pollBox.Text)
	if not n or n <= 0 then
		return DEFAULT_POLL_INTERVAL
	end
	return n
end

-- JSON request helper
local function requestJson(method, url, bodyTable)
	local req = {
		Url = url,
		Method = method,
		Headers = { ["Content-Type"] = "application/json" },
	}
	if bodyTable ~= nil then
		req.Body = HttpService:JSONEncode(bodyTable)
	end

	local ok, resp = pcall(function()
		return HttpService:RequestAsync(req)
	end)
	if not ok then
		return nil, ("RequestAsync error: %s"):format(tostring(resp))
	end
	if not resp.Success then
		return nil, ("HTTP %d %s\n%s"):format(resp.StatusCode or -1, resp.StatusMessage or "", resp.Body or "")
	end

	local json = safeJsonDecode(resp.Body or "")
	if json == nil then
		return nil, ("Response was not JSON.\nBody:\n%s"):format(resp.Body or "")
	end

	return json, nil
end

-- =========================
-- Core: mirrors your Python workflow
-- =========================
local isRunning = false

local function api_fastimage(input_text, poll_interval)
	-- Submit
	local submitUrl = ("%s/%s/api_execute_async/%s"):format(API_URL_BASE, API_VERSION, API_ID)
	local payload = { input_text = input_text }

	local submitJson, submitErr = requestJson("POST", submitUrl, payload)
	if submitErr then error("API submission failed: " .. submitErr) end

	local execution_id = submitJson and submitJson.execution_id
	if type(execution_id) ~= "string" or execution_id == "" then
		error("Missing execution_id in response: " .. prettyJson(submitJson))
	end

	statusLabel.Text = "Execution ID: " .. execution_id:sub(1, 8) .. "..."

	-- Poll status
	local statusUrl = ("%s/%s/api_status/%s"):format(API_URL_BASE, API_VERSION, execution_id)

	while true do
		task.wait(poll_interval)

		local statusJson, statusErr = requestJson("GET", statusUrl, nil)
		if statusErr then error("API status request failed: " .. statusErr) end

		local status = statusJson.status
		statusLabel.Text = "Status: " .. tostring(status)

		if status == "completed" then
			-- returns outputs dict, same as Python
			return statusJson.result and statusJson.result.outputs
		elseif status == "failed" then
			local errObj = statusJson.error or {}
			local error_msg = errObj.error or "Unknown error"
			local node_info = ""

			if errObj.node_name or errObj.node_type then
				node_info = (" (node: %s)"):format(errObj.node_name or errObj.node_type)
			end
			if errObj.node_id then
				local shortId = tostring(errObj.node_id):sub(1, 8)
				node_info = node_info .. (" [id: %s...]"):format(shortId)
			end
			error(("API execution failed: %s%s"):format(error_msg, node_info))
		elseif status ~= "pending" and status ~= "running" then
			error("Unknown execution status: " .. tostring(status))
		end
	end
end

local function setUiBusy(busy)
	isRunning = busy
	runButton.Active = not busy
	runButton.AutoButtonColor = not busy
	runButton.Text = busy and "Running…" or "Generate"
end

runButton.MouseButton1Click:Connect(function()
	if isRunning then return end

	setUiBusy(true)
	metaBox.Text = ""
	printButton.Visible = false
	lastDownloadUrl = nil
	statusLabel.Text = "Submitting…"

	local text = inputBox.Text or ""
	local poll = parsePollInterval()

	task.spawn(function()
		local ok, resultOrErr = pcall(function()
			-- 1) Generate -> outputs
			local outputs = api_fastimage(text, poll)

			-- Expecting: outputs["output_image"] = file_id (like your Python)
			local file_id = outputs and outputs.output_image
			if type(file_id) ~= "string" or file_id == "" then
				error("No outputs.output_image file_id found. Outputs:\n" .. prettyJson(outputs))
			end

			-- 2) Build download URL (don't download - Roblox can't display it anyway)
			local downloadUrl = ("%s/%s/download_binary_result/%s/%s"):format(
				API_URL_BASE, API_VERSION, API_ID, file_id
			)

			return {
				outputs = outputs,
				file_id = file_id,
				download_url = downloadUrl,
			}
		end)

		if ok then
			statusLabel.Text = "Completed!"
			
			-- Store in history
			table.insert(generationHistory, 1, {
				prompt = text,
				file_id = resultOrErr.file_id,
				url = resultOrErr.download_url,
				time = os.date("%H:%M:%S"),
			})
			-- Keep only last 10
			while #generationHistory > 10 do
				table.remove(generationHistory)
			end
			
			-- Show URL prominently - make it easy to select/copy
			metaBox.Text = resultOrErr.download_url
			
			lastDownloadUrl = resultOrErr.download_url
			printButton.Visible = true
			
			-- Auto-print to Output for convenience
			print("=== Atlas FastImage Generated ===")
			print("Prompt: " .. text)
			print("File ID: " .. (resultOrErr.file_id or "unknown"))
			print("Download URL (copy this):")
			print(resultOrErr.download_url)
			print("=================================")
		else
			statusLabel.Text = "Failed."
			metaBox.Text = tostring(resultOrErr)
			printButton.Visible = false
		end

		setUiBusy(false)
	end)
end)

printButton.MouseButton1Click:Connect(function()
	if lastDownloadUrl then
		print("=== Atlas FastImage URL ===")
		print(lastDownloadUrl)
		print("Copy the URL above and paste in your browser to download.")
		print("===========================")
		statusLabel.Text = "URL printed to Output window!"
	end
end)

historyButton.MouseButton1Click:Connect(function()
	if #generationHistory == 0 then
		print("=== Atlas FastImage History ===")
		print("No images generated yet this session.")
		print("===============================")
	else
		print("=== Atlas FastImage History ===")
		for i, entry in ipairs(generationHistory) do
			print(string.format("[%d] %s - \"%s\"", i, entry.time, entry.prompt:sub(1, 30)))
			print("    " .. entry.url)
		end
		print("===============================")
	end
	statusLabel.Text = "History printed to Output window!"
end)

toggleButton.Click:Connect(function()
	widget.Enabled = not widget.Enabled
end)

widget:GetPropertyChangedSignal("Enabled"):Connect(function()
	toggleButton:SetActive(widget.Enabled)
end)
