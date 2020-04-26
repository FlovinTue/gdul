using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Xml;
using System.Windows.Forms.DataVisualization.Charting;

namespace job_time_set_view
{
    using job_info = List<Tuple<string, string>>;

    public partial class job_time_set_view : Form
    {
        private Dictionary<string, job_info> m_jobInfos;

        // dataSource -> dataVariation -> value list with links to corresponding job_info
        private Dictionary<string, Dictionary<string, List<Tuple<float, string>>>> m_dataSources;

        // for string parse conversion
        private System.Globalization.CultureInfo m_enCultureInfo;

        public job_time_set_view()
        {
            InitializeComponent();

            m_jobInfos = new Dictionary<string, job_info>();
            m_dataSources = new Dictionary<string, Dictionary<string, List<Tuple<float, string>>>>();

            m_enCultureInfo = new System.Globalization.CultureInfo("en-US");
        }

        private void job_time_set_view_on_drag_drop(object sender, System.Windows.Forms.DragEventArgs e)
        {
            this.DataSource.Items.Clear();
            this.tabControl1.TabPages.Clear();
            m_jobInfos.Clear();
            m_dataSources.Clear();

            var dropped = e.Data.GetData(DataFormats.FileDrop);
            if (dropped.GetType() != typeof(System.String[]))
            {
                e.Effect = DragDropEffects.None;
                return;
            }
            string[] filesDropped = dropped as string[];
            string first = filesDropped[0];

            if (!first.Contains("job_time_sets.xml"))
            {
                e.Effect = DragDropEffects.None;
                return;
            }

            StreamReader streamReader = new StreamReader(first, Encoding.ASCII);

            XmlDocument newDoc = new XmlDocument();
            newDoc.Load(streamReader);

            parse_xml(newDoc);

            foreach (var dataSource in m_dataSources)
                this.DataSource.Items.Add(dataSource.Key);
        }
        private void job_time_set_view_drag_enter(object sender, System.Windows.Forms.DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
                e.Effect = DragDropEffects.All;
            else
                e.Effect = DragDropEffects.None;
        }
        private void parse_xml(XmlDocument document)
        {
            if (!document.HasChildNodes)
                return;

            XmlNodeList jobs = document.GetElementsByTagName("jobs");

            if (jobs == null)
                return;

            for(var i = 0; i < jobs[0].ChildNodes.Count; ++i)
            {
                XmlNode job = jobs[0].ChildNodes[i];
                XmlAttributeCollection jobAttrib = job.Attributes;
                XmlNode jobIdentifier = jobAttrib.Item(0);
                string jobIdStr = jobIdentifier.Value.ToString();
                job_info info = new job_info();

                m_jobInfos.Add(jobIdStr, info);

                info.Add(new Tuple<string, string>(jobIdentifier.Name + ": ",  jobIdentifier.Value.ToString()));

                for(var mem = 0; mem < job.ChildNodes.Count; ++mem)
                {
                    XmlNode member = job.ChildNodes[mem];

                    if (member.Name == "time_set")
                    {
                        string setName = member.Attributes.Item(0).Value.ToString();
                        if (!m_dataSources.ContainsKey(setName))
                            m_dataSources.Add(setName, new Dictionary<string, List<Tuple<float, string>>>());

                        var dataSource = m_dataSources[setName];
                        for (var tsIndex = 0; tsIndex < member.ChildNodes.Count; ++tsIndex)
                        {
                            XmlNode tsMember = member.ChildNodes[tsIndex];

                            string tsMemberName = tsMember.Name;

                            if (!dataSource.ContainsKey(tsMemberName))
                                dataSource.Add(tsMemberName, new List<Tuple<float, string>>());

                            string valueStr = tsMember.FirstChild.Value;
                            float value = float.Parse(valueStr, m_enCultureInfo);

                            dataSource[tsMemberName].Add(new Tuple<float, string>(value, jobIdStr));
                            info.Add(new Tuple<string, string>(setName + "_",  tsMemberName + ": " + valueStr));
                        }
                        continue;
                    }

                    info.Add(new Tuple<string, string>(member.Name + ": ", member.FirstChild.Value.ToString()));
                }
            }
        }
        private void generate_tabs(string dataSource)
        {
            if (!m_dataSources.ContainsKey(dataSource))
                return;

            var ds = m_dataSources[dataSource];

            List<Color> someColors = new List<Color>();
            someColors.Add(Color.DarkBlue);
            someColors.Add(Color.DarkOrange);
            someColors.Add(Color.DarkViolet);
            someColors.Add(Color.DarkGreen);
            someColors.Add(Color.DarkRed);

            foreach (var variationSource in ds)
            {
                TabPage page = new TabPage();
                page.Location = new System.Drawing.Point(4, 22);
                page.Size = new System.Drawing.Size(846, 669);
                page.UseVisualStyleBackColor = true;
                page.Text = variationSource.Key;

                Chart chart = new Chart();
                
                ((System.ComponentModel.ISupportInitialize)(chart)).BeginInit();

                chart.Location = new System.Drawing.Point(4, 22);
                chart.Size = new System.Drawing.Size(846, 669);
                chart.TabIndex = 5;
                
                ChartArea chartArea = new ChartArea();
                chartArea.Name = "ChartArea1";
                chartArea.AxisX.ScaleView.Zoomable = true;
                chartArea.BackColor = Color.FromArgb(245, 245, 255);

                chart.ChartAreas.Add(chartArea);
                chart.Click += chart_clicked;
                chart.MouseWheel += on_mouse_wheel;

                chart.Anchor = AnchorStyles.Top | AnchorStyles.Right | AnchorStyles.Left | AnchorStyles.Bottom;

                Legend legend = new Legend();
                legend.Name = "Legend1";
                chart.Legends.Add(legend);
                
                Series series = new Series();
                series.Name = variationSource.Key;
                series.Legend = "Legend1";
                series.ChartArea = "ChartArea1";
                series.YValueType = ChartValueType.Double;
                series.YAxisType = AxisType.Primary;
                
                Font f = new Font(series.Font.FontFamily, 8.0f);
                series.Font = f;
                series.SmartLabelStyle.MaxMovingDistance = 1000.0f;
                series.SmartLabelStyle.MinMovingDistance = 100.0f;
                series.SmartLabelStyle.MovingDirection = LabelAlignmentStyles.Top | LabelAlignmentStyles.TopRight;
                series.SmartLabelStyle.CalloutLineWidth = 2;

                int colorCycle = 0;

                foreach (var data in variationSource.Value)
                {
                    job_info info = m_jobInfos[data.Item2];

                    DataPoint point = new DataPoint();
                    point.SetValueY(data.Item1);
                    point.Label = info[1].Item2;

                    StringBuilder toolTip = new StringBuilder();
                    
                    foreach(var infoPair in info)
                    {
                        toolTip.Append(infoPair.Item1);
                        toolTip.Append(infoPair.Item2);
                        toolTip.Append("\n");
                    }
                    point.ToolTip = toolTip.ToString();
                    point.SetCustomProperty("job", data.Item2);

                    series.Points.Add(point);
                }
                series.Sort(PointSortOrder.Descending);

                foreach(var point in series.Points)
                {
                    point.LabelForeColor = someColors[colorCycle++ % someColors.Count];
                    point.MarkerColor = point.LabelForeColor;
                }

                chart.Series.Add(series);

                ((System.ComponentModel.ISupportInitialize)(chart)).EndInit();

                page.Controls.Add(chart);

                this.tabControl1.TabPages.Add(page);
                page.Enter += new System.EventHandler(this.on_tab_enter);
            }
            this.tabControl1.Update();
            on_tab_enter(this.tabControl1.TabPages[0], null);
        }

        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            foreach(TabPage tab in this.tabControl1.TabPages)
            {
                foreach (Chart chart in tab.Controls)
                {
                    chart.Click -= chart_clicked;
                    chart.MouseWheel -= on_mouse_wheel;
                }
            }

            this.tabControl1.TabPages.Clear();

            var selectedItem = this.DataSource.SelectedItem;

            if (selectedItem == null)
                return;

            generate_tabs(selectedItem as string);
        }

        private void chart_clicked(object sender, EventArgs e)
        {
            this.ItemProperties.Items.Clear();

            Chart chart = sender as Chart;
            MouseEventArgs args = e as MouseEventArgs;

            if (chart == null || args == null)
                return;

            HitTestResult hitTestResult = chart.HitTest(args.X, args.Y);

            DataPoint hitPoint = hitTestResult.Object as DataPoint;

            if (hitPoint == null)
                return;

            string job = hitPoint.GetCustomProperty("job");

            job_info info = m_jobInfos[job];
            
            foreach(var infoPair in info)
            {
                this.ItemProperties.Items.Add(infoPair.Item1 + infoPair.Item2);
            }
        }

        private void on_mouse_wheel(object sender, EventArgs e)
        {
            Chart chart = sender as Chart;
            MouseEventArgs args = e as MouseEventArgs;

            if (chart == null || args == null)
                return;

            var xAxis = chart.ChartAreas[0].AxisX;

            if (args.Delta < 0)
            {
                chart_zoom(chart, 1 / 0.8f);
                return;
            }

            chart_zoom(chart, 0.8f);
        }
        private void on_tab_enter(object sender, EventArgs e)
        {
            if (sender == null)
                return;

            TabPage page = sender as TabPage;

            List<Chart> charts = page.Controls.OfType<Chart>().ToList();

            if (charts.Count == 0)
                return;

            Series series = charts[0].Series[0];

            float zoom = 25.0f / (float)series.Points.Count;

            charts[0].ChartAreas[0].AxisX.ScaleView.ZoomReset();
            charts[0].ChartAreas[0].AxisX.ScaleView.Zoom(0, 25.0f);

            charts[0].Refresh();
        }
        private void chart_zoom(Chart chart, float zoomMulti)
        {
            var xAxis = chart.ChartAreas[0].AxisX;

            var xMin = xAxis.ScaleView.ViewMinimum;
            var xMax = xAxis.ScaleView.ViewMaximum;

            var posXStart = 0;
            var posXFinish = 0 + (xMax - xMin) * zoomMulti;

            xAxis.ScaleView.Zoom(posXStart, posXFinish);
        }
        private void mouse_enter_data_point(object sender, MouseEventArgs e)
        {

        }
        private void mouse_leave_data_point(object sender, MouseEventArgs e)
        {

        }
    }
}
