using System.Runtime.InteropServices;
using VoidCare.Cli.Commands;
using VoidCare.Cli.Interactive;
using VoidCare.Cli.Rendering;
using VoidCare.Cli.Runtime;
using VoidCare.Infrastructure.Services;
using VoidCare.Core.Services;
using VoidCare.Core.Models;

static bool IsStandaloneConsoleWindow()
{
    var processIds = new uint[16];
    return NativeMethods.GetConsoleProcessList(processIds, processIds.Length) <= 1;
}

static void PauseBeforeExitIfNeeded(CommandOptions options)
{
    if (options.Json || Console.IsInputRedirected || !IsStandaloneConsoleWindow())
    {
        return;
    }

    Console.WriteLine();
    Console.Write("Press Enter to exit...");
    Console.ReadLine();
}

var renderer = new TerminalRenderer();
var parser = InvocationParser.Parse(args);

var pathService = new PathService();
var stateStore = new StateStore(pathService);
var logger = new ActionLogger(pathService);
var adminService = new AdminService();
var processRunner = new ProcessRunner();
var powerShellRunner = new PowerShellRunner(processRunner);
var restorePointService = new RestorePointService(powerShellRunner);
var explorerService = new ExplorerService();
var hashService = new HashService();
var signatureVerifier = new FileSignatureVerifier();
var antivirusDiscoveryService = new AntivirusDiscoveryService(powerShellRunner);
var defenderService = new DefenderService(processRunner, powerShellRunner);
var persistenceService = new PersistenceService(signatureVerifier, processRunner, powerShellRunner, pathService);
var suspiciousFileService = new SuspiciousFileService(pathService, signatureVerifier, hashService, processRunner);
var optimizationService = new OptimizationService(pathService, processRunner);
var appsService = new AppsService(powerShellRunner, processRunner);
var bloatCatalogService = new BloatCatalogService();

var dispatcher = new CommandDispatcher(
    pathService,
    stateStore,
    logger,
    adminService,
    restorePointService,
    explorerService,
    antivirusDiscoveryService,
    defenderService,
    persistenceService,
    suspiciousFileService,
    optimizationService,
    appsService,
    bloatCatalogService);

if (parser.LaunchInteractive)
{
    if (parser.Options.Json)
    {
        var failure = new CommandResult
        {
            Success = false,
            ExitCode = 2,
            Command = "interactive",
            Summary = "Interactive mode does not support --json.",
        };
        new JsonEventWriter(Console.Out).WriteResult(failure);
        return failure.ExitCode;
    }

    if (!parser.Options.NoBanner)
    {
        renderer.RenderBanner();
    }

    var menu = new InteractiveMenu(dispatcher, renderer, parser.Options);
    return await menu.RunAsync();
}

var progressHandler = parser.Options.Json
    ? new Action<ProgressEvent>(new JsonEventWriter(Console.Out).WriteProgress)
    : new Action<ProgressEvent>(evt => renderer.RenderProgress(evt, parser.Options.Verbose));

var invocationResult = dispatcher.IsLongRunning(parser.Arguments) && !parser.Options.Json
    ? await renderer.RunWithStatusAsync(dispatcher.GetStatusText(parser.Arguments), () => dispatcher.ExecuteAsync(parser, renderer, progressHandler))
    : await dispatcher.ExecuteAsync(parser, renderer, progressHandler);

if (parser.Options.Json)
{
    new JsonEventWriter(Console.Out).WriteResult(invocationResult);
}
else
{
    renderer.Render(invocationResult, parser.Options);
    PauseBeforeExitIfNeeded(parser.Options);
}

dispatcher.RecordOutcome(invocationResult);
return invocationResult.ExitCode;
