using System.Globalization;
using System.Windows.Data;

namespace VoidCare.Wpf.Converters;

public sealed class PageMatchConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
    {
        if (values.Length < 2)
        {
            return false;
        }

        var current = values[0]?.ToString() ?? string.Empty;
        var target = values[1]?.ToString() ?? string.Empty;
        return string.Equals(current, target, StringComparison.OrdinalIgnoreCase);
    }

    public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
    {
        throw new NotSupportedException();
    }
}
