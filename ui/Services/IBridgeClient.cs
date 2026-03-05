using System.Text.Json.Nodes;

namespace VoidCare.Wpf.Services;

public interface IBridgeClient : IAsyncDisposable
{
    event EventHandler<BridgeEvent>? EventReceived;

    bool IsConnected { get; }
    Task ConnectAsync(CancellationToken cancellationToken = default);
    Task DisconnectAsync();
    Task<BridgeResponse> SendRequestAsync(string method, JsonObject? args = null, CancellationToken cancellationToken = default);
}
