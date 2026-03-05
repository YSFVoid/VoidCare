using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Animation;
using VoidCare.Wpf.ViewModels;
using VoidCare.Wpf.Views.Pages;

namespace VoidCare.Wpf.Views;

public partial class ShellWindow : Window
{
    private readonly MainViewModel _viewModel;
    private readonly Dictionary<string, UserControl> _pageCache = new(StringComparer.OrdinalIgnoreCase);

    public ShellWindow()
    {
        InitializeComponent();
        _viewModel = new MainViewModel();
        DataContext = _viewModel;
        _viewModel.PropertyChanged += OnViewModelPropertyChanged;
        ShowCurrentPage(false);
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        base.OnClosing(e);
        _viewModel.PropertyChanged -= OnViewModelPropertyChanged;
        _viewModel.DisposeAsync().AsTask().GetAwaiter().GetResult();
    }

    private void OnViewModelPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName == nameof(MainViewModel.CurrentPage))
        {
            ShowCurrentPage(true);
        }
    }

    private void ShowCurrentPage(bool animated)
    {
        var page = ResolvePage(_viewModel.CurrentPage);
        if (!animated)
        {
            PageContentHost.Content = page;
            return;
        }

        var fadeOut = new DoubleAnimation(1.0, 0.0, TimeSpan.FromMilliseconds(120));
        fadeOut.Completed += (_, _) =>
        {
            PageContentHost.Content = page;
            var fadeIn = new DoubleAnimation(0.0, 1.0, TimeSpan.FromMilliseconds(180));
            PageContentHost.BeginAnimation(OpacityProperty, fadeIn);
        };
        PageContentHost.BeginAnimation(OpacityProperty, fadeOut);
    }

    private UserControl ResolvePage(string page)
    {
        if (_pageCache.TryGetValue(page, out var existing))
        {
            return existing;
        }

        UserControl control = page switch
        {
            "Security" => new SecurityPage(),
            "Optimize" => new OptimizePage(),
            "Gaming" => new GamingPage(),
            "Apps" => new AppsPage(),
            "About" => new AboutPage(),
            _ => new DashboardPage(),
        };

        control.DataContext = _viewModel;
        _pageCache[page] = control;
        return control;
    }
}
