function plot_friction_results(T, steady, diagnostics, comparison, cfg, outDir)
%PLOT_FRICTION_RESULTS Create the standard figures and opt-in diagnostics.
phaseColors = [0.0000 0.4470 0.7410; 0.8500 0.3250 0.0980; ...
               0.9290 0.6940 0.1250; 0.4940 0.1840 0.5560];
phaseNames = {'Phase 1','Phase 2','Phase 3','Phase 4'};
criterionNames = {'Strict','Relaxed 1','Relaxed 2'};
criterionMarkers = {'o','^','s'};
classNames = {'MOVING','STATIONARY','UNCERTAIN'};
active = T.phase>=1 & T.phase<=4 & isfinite(T.speed_rad_s) & isfinite(T.feedback_torque_estimate_Nm);
if cfg.plot.show_zero_torque
    nonzeroCommand = true(height(T),1);
else
    nonzeroCommand = abs(T.target_torque_Nm)>cfg.plot.zero_torque_tolerance_Nm;
end

if cfg.plot.raw_velocity_torque
    f=figure('Name','Raw velocity-torque data','Color','w'); hold on;
    for ph=1:4
        ix=active & nonzeroCommand & T.phase==ph;
        scatter(T.speed_rad_s(ix),T.feedback_torque_estimate_Nm(ix),5,phaseColors(ph,:), ...
            'filled','DisplayName',phaseNames{ph},'MarkerFaceAlpha',0.35,'MarkerEdgeAlpha',0.35);
    end
    grid on; xlabel('Speed [rad/s]'); ylabel('Feedback estimated torque [Nm]');
    title('Raw velocity-torque data'); legend('Location','best');
    saveas(f,fullfile(outDir,'raw_velocity_torque.png'));
end

if cfg.plot.steady_velocity_torque
    f=figure('Name','Steady-state comparison','Color','w'); hold on;
    for ph=1:4
        for ci=1:numel(criterionNames)
            if cfg.plot.show_moving_only || ~cfg.plot.show_motion_classification
                classIndices = 1;
            else
                classIndices = 1:numel(classNames);
            end
            for cl=classIndices
                ix = comparison.phase==ph & comparison.criterion==criterionNames{ci};
                if cfg.plot.show_moving_only || ~cfg.plot.show_motion_classification
                    if cfg.plot.show_moving_only
                        ix = ix & comparison.classification=="MOVING";
                    end
                    clFace = phaseColors(ph,:);
                else
                    ix = ix & comparison.classification==classNames{cl};
                    if cl==1, clFace=phaseColors(ph,:);       % MOVING: filled
                    elseif cl==2, clFace='none';              % STATIONARY: hollow
                    else, clFace=[0.75 0.75 0.75]; end        % UNCERTAIN: gray fill
                end
                if ~cfg.plot.show_zero_torque
                    ix = ix & abs(comparison.target_torque_Nm)>cfg.plot.zero_torque_tolerance_Nm;
                end
                ix = ix & isfinite(comparison.mean_speed_rad_s) & ...
                    isfinite(comparison.mean_feedback_torque_Nm);
                if ~any(ix), continue; end
                scatter(comparison.mean_speed_rad_s(ix),comparison.mean_feedback_torque_Nm(ix), ...
                    54,phaseColors(ph,:),criterionMarkers{ci},'MarkerFaceColor',clFace, ...
                    'MarkerEdgeColor',phaseColors(ph,:),'MarkerFaceAlpha',0.55, ...
                    'MarkerEdgeAlpha',0.85,'HandleVisibility','off');
            end
        end
    end
    % Separate proxy entries make the two encodings explicit: color=phase,
    % marker=threshold criterion. Classification gets a third key when shown.
    proxy = gobjects(0,1); labels = {};
    for ph=1:4
        proxy(end+1,1)=scatter(NaN,NaN,54,phaseColors(ph,:),'o','filled'); %#ok<AGROW>
        labels{end+1}=phaseNames{ph}; %#ok<AGROW>
    end
    for ci=1:numel(criterionNames)
        proxy(end+1,1)=scatter(NaN,NaN,54,[0.2 0.2 0.2],criterionMarkers{ci},'filled'); %#ok<AGROW>
        labels{end+1}=criterionNames{ci}; %#ok<AGROW>
    end
    if ~cfg.plot.show_moving_only && cfg.plot.show_motion_classification
        classFaces={ [0.2 0.2 0.2], 'none', [0.75 0.75 0.75] };
        for cl=1:3
            proxy(end+1,1)=scatter(NaN,NaN,54,[0.2 0.2 0.2],'o', ...
                'MarkerFaceColor',classFaces{cl},'MarkerEdgeColor',[0.2 0.2 0.2]); %#ok<AGROW>
            labels{end+1}=classNames{cl}; %#ok<AGROW>
        end
    end
    grid on; xlabel('Mean speed [rad/s]'); ylabel('Mean feedback torque [Nm]');
    if cfg.plot.show_moving_only
        title('Steady-state comparison (MOVING points)');
    else
        title('Steady-state comparison');
    end
    legend(proxy,labels,'Location','bestoutside');
    saveas(f,fullfile(outDir,'steady_velocity_torque.png'));
end

if cfg.plot.steady_interval_timeseries || cfg.plot.steady_metrics
    select=diagnostics.Accepted;
    if ~isempty(cfg.plot.diagnostic_phase), select=select & diagnostics.Phase==cfg.plot.diagnostic_phase; end
    if ~isempty(cfg.plot.diagnostic_step_index), select=select & diagnostics.StepIndex==cfg.plot.diagnostic_step_index; end
    chosen=diagnostics(select,:);
    if cfg.plot.steady_interval_timeseries
        f=figure('Name','Selected steady intervals','Color','w');
        yyaxis left; plot(T.actual_us*1e-6,T.speed_rad_s,'Color',[0.1 0.35 0.75]); ylabel('Speed [rad/s]'); hold on;
        yyaxis right; plot(T.actual_us*1e-6,T.target_torque_Nm,'Color',[0.8 0.3 0.2]); ylabel('Command torque [Nm]'); hold on;
        for k=1:height(chosen), xline(chosen.SelectedWindowStartTimeS(k),':'); xline(chosen.SelectedWindowEndTimeS(k),'--'); end
        xlabel('Time [s]'); grid on; title('Accepted Strict steady-state windows');
        saveas(f,fullfile(outDir,'steady_interval_timeseries.png'));
    end
    if cfg.plot.steady_metrics
        f=figure('Name','Steady-state metrics','Color','w'); tiledlayout(2,1);
        nexttile; scatter(chosen.SelectedWindowEndTimeS,chosen.EstimatedAccelerationRadS2,18,'filled'); hold on;
        yline(cfg.steady.max_abs_accel_rad_s2,'r--'); yline(-cfg.steady.max_abs_accel_rad_s2,'r--'); ylabel('Acceleration [rad/s^2]'); grid on;
        nexttile; scatter(chosen.SelectedWindowEndTimeS,chosen.SpeedStdRadS,18,'filled'); yline(cfg.steady.max_speed_std_rad_s,'r--');
        ylabel('Speed std [rad/s]'); xlabel('Time [s]'); grid on;
        saveas(f,fullfile(outDir,'steady_metrics.png'));
    end
end

if cfg.plot.steady_threshold_distributions
    hasData = ~isempty(comparison) && any(isfinite(comparison.estimated_acceleration_rad_s2));
    if hasData
        f=figure('Name','Steady threshold distributions','Color','w'); tiledlayout(2,1);
        nexttile; hold on;
        for ci=1:numel(criterionNames)
            ix=comparison.criterion==criterionNames{ci} & isfinite(comparison.estimated_acceleration_rad_s2);
            if any(ix), histogram(abs(comparison.estimated_acceleration_rad_s2(ix)), ...
                'DisplayStyle','stairs','LineWidth',1.4,'DisplayName',criterionNames{ci}); end
        end
        xlabel('|Estimated acceleration| [rad/s^2]'); ylabel('Count'); grid on; legend('Location','best');
        nexttile; hold on;
        for ci=1:numel(criterionNames)
            ix=comparison.criterion==criterionNames{ci} & isfinite(comparison.speed_std_rad_s);
            if any(ix), histogram(comparison.speed_std_rad_s(ix), ...
                'DisplayStyle','stairs','LineWidth',1.4,'DisplayName',criterionNames{ci}); end
        end
        xlabel('Speed standard deviation [rad/s]'); ylabel('Count'); grid on; legend('Location','best');
        saveas(f,fullfile(outDir,'steady_threshold_distributions.png'));
    end
end
end
