using System.Text.Json.Nodes;

namespace VoidCare.Wpf.Services;

public sealed class BridgeRequest
{
    public string Type { get; set; } = "request";
    public string Id { get; set; } = string.Empty;
    public string Method { get; set; } = string.Empty;
    public JsonObject Args { get; set; } = new();
}

public sealed class BridgeResponse
{
    public string Type { get; set; } = "response";
    public string Id { get; set; } = string.Empty;
    public bool Ok { get; set; }
    public JsonNode? Result { get; set; }
    public JsonObject? Error { get; set; }
}

public sealed class BridgeEvent
{
    public string Type { get; set; } = "event";
    public string Event { get; set; } = string.Empty;
    public JsonObject Payload { get; set; } = new();
}
